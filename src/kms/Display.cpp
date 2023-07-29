#include "Display.hpp"
#include "Edid.hpp"
#include "kms.hpp"
#include <array>
#include <cstdint>
#include <gbm.h>

namespace glplay::kms {

  void Display::buffer_egl_destroy(int adapterFD, egl::EGLDevice &eglDevice, Buffer &buffer) {
    drmModeRmFB(adapterFD, buffer.fb_id);
    static PFNEGLDESTROYIMAGEKHRPROC destroy_img = nullptr;
    EGLBoolean ret = 0;

    ret = eglMakeCurrent(eglDevice.egl_dpy, EGL_NO_SURFACE, EGL_NO_SURFACE,
            eglDevice.ctx);
    assert(ret);

    if (destroy_img == nullptr) {
      destroy_img = reinterpret_cast<PFNEGLDESTROYIMAGEKHRPROC>(eglGetProcAddress("eglDestroyImageKHR"));
    }
    assert(destroy_img);

    destroy_img(eglDevice.egl_dpy, buffer.gbm.img);
    glDeleteFramebuffers(1, &buffer.gbm.fbo_id);
    glDeleteTextures(1, &buffer.gbm.tex_id);
    gbm_bo_destroy(buffer.gbm.bo);
  }

  void Display::failOnBOCreationError(Buffer &buffer, std::array<int, 4> dma_buf_fds) {
    gbm_bo_destroy(buffer.gbm.bo);
    for (const auto& dma_buf_fd : dma_buf_fds) {
      if (dma_buf_fd != -1) {
        close(dma_buf_fd);
      }
    }
    throw std::runtime_error("failed to create BO\n");
  }

  auto Display::findCrtcForEncoder(int adapterFD, drm::Resources &resources, drm::Encoder &encoder) -> drm::Crtc {
		for(int idx = 0; idx < resources->count_crtcs; idx++) {
      if (resources->crtcs[idx] == encoder->crtc_id) {
				return drm::make_crtc_ptr(adapterFD, resources->crtcs[idx]);
			}
		}
		throw std::runtime_error("Unable to find crtc for encoder");
	}

 	auto Display::findPrimaryPlaneForCrtc() -> drm::Plane {
		for (auto plane : planes) {
			if(plane->crtc_id == crtc->crtc_id && plane->fb_id == crtc->buffer_id) {
				return plane;
			}
		}
		throw std::runtime_error("Unable to find plane for crtc");
	}

  auto Display::findEncoderForConnector(int adapterFD, drm::Resources &resources, drm::Connector &connector) -> drm::Encoder {
		for(int idx = 0; idx < resources->count_encoders; idx++) {
      if (resources->encoders[idx] == connector->encoder_id) {
				return drm::make_encoder_ptr(adapterFD, resources->encoders[idx]);
			}
		}
		throw std::runtime_error("Unable to find encoder for connector");
	}

  Display::Display(int adapterFD, uint32_t connectorId, drm::Resources &resources):
    explicitFencing(false), connector(drm::make_connetor_ptr(adapterFD, connectorId)) {
    auto encoder = findEncoderForConnector(adapterFD, resources, connector);
    this->crtc = findCrtcForEncoder(adapterFD, resources, encoder);

    auto planeResources = drm::make_plane_resources_ptr(adapterFD);

    if (resources->count_crtcs <= 0 || resources->count_connectors <= 0 ||
				resources->count_encoders <= 0 || planeResources->count_planes <= 0) {
      throw std::runtime_error("device fd is not a KMS device");
		}

    for(uint32_t idx = 0; idx < planeResources->count_planes; ++idx) {
			planes.emplace_back(drm::make_plane_ptr(adapterFD, planeResources->planes[idx]));
    }

    this->primary_plane = findPrimaryPlaneForCrtc();
    auto refresh = ((crtc->mode.clock * 1000000LL / crtc->mode.htotal) +
		  (crtc->mode.vtotal / 2)) / crtc->mode.vtotal;
    
    name = ((connector->connector_type < drm::connectorTypes.size()) ?
		 	drm::connectorTypes.at(connector->connector_type) :
			"UNKNOWN") + "-" + std::to_string(connector->connector_type_id);

    refreshIntervalNsec = millihzToNsec(refresh);
    
    mode_blob_id = drm::mode_blob_create(adapterFD, &crtc->mode);
    
    auto *planeProps = drmModeObjectGetProperties(adapterFD, primary_plane->plane_id, DRM_MODE_OBJECT_PLANE);
    drm::drm_property_info_populate(adapterFD, drm::plane_props, this->props.plane, drm::plane_props.size(), planeProps);
    this->plane_formats_populate(adapterFD, planeProps);

    auto *crtProps = drmModeObjectGetProperties(adapterFD, crtc->crtc_id, DRM_MODE_OBJECT_CRTC);
    drm::drm_property_info_populate(adapterFD, drm::crtc_props, this->props.crtc, drm::crtc_props.size(), crtProps);

    auto *connectorProps = drmModeObjectGetProperties(adapterFD, connector->connector_id, DRM_MODE_OBJECT_CONNECTOR);
    drm::drm_property_info_populate(adapterFD, drm::connector_props, this->props.connector, drm::connector_props.size(), connectorProps);

    this->get_edid(adapterFD, connectorProps);

    /*
	  * Set if we support explicit fencing inside KMS; the EGL renderer will
	  * clear this if it doesn't support it.
	  */
	  this->explicitFencing =
		((this->props.plane.at(drm::WDRM_PLANE_IN_FENCE_FD).prop_id != 0U) &&
		 (this->props.crtc.at(drm::WDRM_CRTC_OUT_FENCE_PTR).prop_id != 0U));

  }

  void Display::createEGLBuffers(int adapterFD, bool adapterSupportsFBModifiers, egl::EGLDevice &eglDevice, gbm::GBMDevice &gbmDevice) {    
    for (int idx = 0; idx < BUFFER_QUEUE_DEPTH; idx++) {
      Buffer buffer = createEGLBuffer(adapterFD, adapterSupportsFBModifiers, eglDevice, gbmDevice);
      std::array<uint64_t, 4> buffer_modifiers = { 0, };

      for (int i = 0; buffer.gem_handles.at(i); i++) {
        buffer_modifiers.at(i) = buffer.modifier;
        debug("[GEM:%" PRIu32 "]: %u x %u %s buffer (plane %d), pitch %u\n",
              buffer.gem_handles.at(i), buffer.width, buffer.height,
              "GBM",
              i, buffer.pitches.at(i));
      }

      int err = 0;
  
     	/*
      * Wrap our GEM buffer in a KMS framebuffer, so we can then attach it
      * to a plane.
      *
      * drmModeAddFB2 accepts multiple image planes (not to be confused with
      * the KMS plane objects!), for images which have multiple buffers.
      * For example, YUV images may have the luma (Y) components in a
      * separate buffer to the chroma (UV) components.
      *
      * When using modifiers (which we do not for dumb buffers), we can also
      * have multiple planes even for RGB images, as image compression often
      * uses an auxiliary buffer to store compression metadata.
      *
      * Dumb buffers are always strictly single-planar, so we do not need
      * the extra planes nor the offset field.
      *
      * AddFB2WithModifiers takes a list of modifiers per plane, however
      * the kernel enforces that they must be the same for each plane
      * which is there, and 0 for everything else.
      */
      if (buffer.supportsFBModifiers) {
        err = drmModeAddFB2WithModifiers(adapterFD,
          buffer.width, buffer.height,
          buffer.format,
          buffer.gem_handles.data(),
          buffer.pitches.data(),
          buffer.offsets.data(),
          buffer_modifiers.data(),
          &buffer.fb_id,
          DRM_MODE_FB_MODIFIERS);
      } else {
        err = drmModeAddFB2(adapterFD,
          buffer.width, buffer.height,
          buffer.format,
          buffer.gem_handles.data(),
          buffer.pitches.data(),
          buffer.offsets.data(),
          &buffer.fb_id,
          0);
      }

      if (err != 0 || buffer.fb_id == 0) {
        error("failed AddFB2 on %u x %u %s (modifier 0x%" PRIx64 ") buffer: %s\n",
          buffer.width, buffer.height,
          "GBM",
          buffer.modifier, strerror(errno));
        Display::buffer_egl_destroy(adapterFD, eglDevice, buffer);
      }
      buffers.push_back(buffer);
    }
  }

  auto Display::createEGLBuffer(int adapterFD, bool adapterSupportsFBModifiers, egl::EGLDevice &eglDevice, gbm::GBMDevice &gbmDevice) -> Buffer {
    static PFNEGLCREATEIMAGEKHRPROC create_img = nullptr;
    static PFNGLEGLIMAGETARGETTEXTURE2DOESPROC target_tex_2d = nullptr;
    std::array<EGLint, 64> attribs = { 0, }; /* see note below about type */
    EGLint nattribs = 0;
    EGLBoolean err = 0;
    int num_planes = 0;
    std::array<int, 4> dma_buf_fds = { -1, -1, -1, -1 };

    Buffer buffer;
    buffer.render_fence_fd = -1;
    buffer.kms_fence_fd = -1;
    buffer.supportsFBModifiers = adapterSupportsFBModifiers;

    /*
    * We have the list of the acceptable modifiers for KMS, which we just
    * pass straight to GBM. GBM takes this list of modifiers, and picks
    * the 'best' modifier according to whatever internal preference the
    * driver wants to use. We can then query the GBM BO to find out which
    * modifier it selected.
    */
    if (buffer.supportsFBModifiers) {
      buffer.gbm.bo = gbm_bo_create_with_modifiers(
        gbmDevice.get(),
        crtc->mode.hdisplay,
        crtc->mode.vdisplay,
        DRM_FORMAT_XRGB8888,
        modifiers.data(),
        modifiers.size());
    }
    if (buffer.gbm.bo == nullptr) {
      /*
      * Fall back to the non-modifier path if we can't create a
      * buffer with modifiers.
      */
      buffer.supportsFBModifiers = false;
      buffer.gbm.bo = gbm_bo_create(
        gbmDevice.get(),
        crtc->mode.hdisplay,
        crtc->mode.vdisplay,
        DRM_FORMAT_XRGB8888,
        GBM_BO_USE_RENDERING | GBM_BO_USE_SCANOUT);
    }
  
    if(buffer.gbm.bo == nullptr) {
      error("failed to create %u x %u BO\n",
		    crtc->mode.hdisplay, crtc->mode.vdisplay);
      throw std::runtime_error("failed to create BO\n");
    }

    /*
    * We can query all the image properties from the GBM BO once we've
    * created it.
    */
    buffer.format = DRM_FORMAT_XRGB8888;
    buffer.width = crtc->mode.hdisplay;
    buffer.height = crtc->mode.vdisplay;
    buffer.modifier = gbm_bo_get_modifier(buffer.gbm.bo);
    num_planes = gbm_bo_get_plane_count(buffer.gbm.bo);
    for (int i = 0; i < num_planes; i++) {
      union gbm_bo_handle handle{};

      /* In hindsight, we got this API wrong. */
      handle = gbm_bo_get_handle_for_plane(buffer.gbm.bo, i);
      if (handle.u32 == 0 || handle.s32 == -1) {
        error("failed to get handle for BO plane %d (modifier 0x%" PRIx64 ")\n",
              i, buffer.modifier);
        Display::failOnBOCreationError(buffer, dma_buf_fds);
      }
      buffer.gem_handles.at(i) = handle.u32;

      dma_buf_fds.at(i) = handle_to_fd(adapterFD, buffer.gem_handles.at(i));
      if (dma_buf_fds.at(i) == -1) {
        error("failed to get file descriptor for BO plane %d (modifier 0x%" PRIx64 ")\n",
              i, buffer.modifier);
        failOnBOCreationError(buffer, dma_buf_fds);
      }

      buffer.pitches.at(i) = gbm_bo_get_stride_for_plane(buffer.gbm.bo, i);
      if (buffer.pitches.at(i) == 0) {
        error("failed to get stride for BO plane %d (modifier 0x%" PRIx64 ")\n",
              i, buffer.modifier);
        failOnBOCreationError(buffer, dma_buf_fds);
      }

      buffer.offsets.at(i) = gbm_bo_get_offset(buffer.gbm.bo, i);
	  }

    /*
    * EGL has two versions of image creation, which are not actually
    * interchangeable: eglCreateImageKHR takes an EGLint for its attrib
    * list, whereas eglCreateImage (core as of 1.5) takes an EGLAttrib
    * list, so we would have to have two different list-population
    * implementations depending on which path we took.
    *
    * To avoid this, just use eglCreateImageKHR, since everyone
    * implements that.
    */
    if (create_img == nullptr) {
      create_img = reinterpret_cast<PFNEGLCREATEIMAGEKHRPROC>(eglGetProcAddress("eglCreateImageKHR"));
    }
    assert(create_img);

    attribs.at(nattribs++) = EGL_WIDTH;
    attribs.at(nattribs++) = buffer.width;
    attribs.at(nattribs++) = EGL_HEIGHT;
    attribs.at(nattribs++) = buffer.height;
    attribs.at(nattribs++) = EGL_LINUX_DRM_FOURCC_EXT;
    attribs.at(nattribs++) = DRM_FORMAT_XRGB8888;
    debug("importing %u x %u EGLImage with %d planes\n", buffer.width, buffer.height, num_planes);

    attribs.at(nattribs++) = EGL_DMA_BUF_PLANE0_FD_EXT;
    attribs.at(nattribs++) = dma_buf_fds[0];
    debug("\tplane 0 FD %d\n", attribs.at(nattribs - 1));
    attribs.at(nattribs++) = EGL_DMA_BUF_PLANE0_OFFSET_EXT;
    attribs.at(nattribs++) = buffer.offsets[0];
    debug("\tplane 0 offset %d\n", attribs.at(nattribs - 1));
    attribs.at(nattribs++) = EGL_DMA_BUF_PLANE0_PITCH_EXT;
    attribs.at(nattribs++) = buffer.pitches[0];
    debug("\tplane 0 pitch %d\n", attribs.at(nattribs - 1));
#ifdef EGL_VERSION_1_5
    if (buffer.supportsFBModifiers) {
      attribs.at(nattribs++) = EGL_DMA_BUF_PLANE0_MODIFIER_HI_EXT;
      attribs.at(nattribs++) = buffer.modifier >> 32;
      debug("\tmodifier hi 0x%" PRIx32 "\n", attribs.at(nattribs - 1));
      attribs.at(nattribs++) = EGL_DMA_BUF_PLANE0_MODIFIER_LO_EXT;
      attribs.at(nattribs++) = buffer.modifier & 0xffffffff;
      debug("\tmodifier lo 0x%" PRIx32 "\n", attribs.at(nattribs - 1));
    }
#endif
    if (num_planes > 1) {
      attribs.at(nattribs++) = EGL_DMA_BUF_PLANE1_FD_EXT;
      attribs.at(nattribs++) = dma_buf_fds[1];
      attribs.at(nattribs++) = EGL_DMA_BUF_PLANE1_OFFSET_EXT;
      attribs.at(nattribs++) = buffer.offsets[1];
      attribs.at(nattribs++) = EGL_DMA_BUF_PLANE1_PITCH_EXT;
      attribs.at(nattribs++) = buffer.pitches[1];
#ifdef EGL_VERSION_1_5
      if (buffer.supportsFBModifiers) {
        attribs.at(nattribs++) = EGL_DMA_BUF_PLANE1_MODIFIER_HI_EXT;
        attribs.at(nattribs++) = buffer.modifier >> 32;
        attribs.at(nattribs++) = EGL_DMA_BUF_PLANE1_MODIFIER_LO_EXT;
        attribs.at(nattribs++) = buffer.modifier & 0xffffffff;
      }
#endif
    }

    if (num_planes > 2) {
      attribs.at(nattribs++) = EGL_DMA_BUF_PLANE2_FD_EXT;
      attribs.at(nattribs++) = dma_buf_fds[2];
      attribs.at(nattribs++) = EGL_DMA_BUF_PLANE2_OFFSET_EXT;
      attribs.at(nattribs++) = buffer.offsets[2];
      attribs.at(nattribs++) = EGL_DMA_BUF_PLANE2_PITCH_EXT;
      attribs.at(nattribs++) = buffer.pitches[2];
#ifdef EGL_VERSION_1_5
      if (buffer.supportsFBModifiers) {
        attribs.at(nattribs++) = EGL_DMA_BUF_PLANE2_MODIFIER_HI_EXT;
        attribs.at(nattribs++) = buffer.modifier >> 32;
        attribs.at(nattribs++) = EGL_DMA_BUF_PLANE2_MODIFIER_LO_EXT;
        attribs.at(nattribs++) = buffer.modifier & 0xffffffff;
      }
#endif
    }

#ifdef EGL_VERSION_1_5
    if (num_planes > 3) {
      attribs.at(nattribs++) = EGL_DMA_BUF_PLANE3_FD_EXT;
      attribs.at(nattribs++) = dma_buf_fds[3];
      attribs.at(nattribs++) = EGL_DMA_BUF_PLANE3_OFFSET_EXT;
      attribs.at(nattribs++) = buffer.offsets[3];
      attribs.at(nattribs++) = EGL_DMA_BUF_PLANE3_PITCH_EXT;
      attribs.at(nattribs++) = buffer.pitches[3];
      if (buffer.supportsFBModifiers) {
        attribs.at(nattribs++) = EGL_DMA_BUF_PLANE3_MODIFIER_HI_EXT;
        attribs.at(nattribs++) = buffer.modifier >> 32;
        attribs.at(nattribs++) = EGL_DMA_BUF_PLANE3_MODIFIER_LO_EXT;
        attribs.at(nattribs++) = buffer.modifier & 0xffffffff;
      }
    }
#endif

    attribs.at(nattribs++) = EGL_NONE;

    err = eglMakeCurrent(eglDevice.egl_dpy, EGL_NO_SURFACE, EGL_NO_SURFACE,
            eglDevice.ctx);
    assert(err);
    
    /*
    * Create an EGLImage from our GBM BO, which will give EGL and GLES
    * the ability to use it as a render target.
    */
    buffer.gbm.img = create_img(eglDevice.egl_dpy, EGL_NO_CONTEXT,
      EGL_LINUX_DMA_BUF_EXT, nullptr, attribs.data());
    if (buffer.gbm.img == nullptr) {
      error("failed to create EGLImage for %u x %u BO (modifier 0x%" PRIx64 ")\n",
        buffer.width, buffer.height, buffer.modifier);
      failOnBOCreationError(buffer, dma_buf_fds);
    }

    /*
    * EGL does not take ownership of the dma-buf file descriptors and will
    * clone them internally. We don't need the file descriptors after
    * they've been imported, so we can just close them now.
    */
    for (int i = 0; i < num_planes; i++) {
      close(dma_buf_fds.at(i));
    }

    /*
    * Bind the EGLImage to a GLES texture unit, then bind that texture
    * to a GL framebuffer object, so we can use it to render into.
    */
    glGenTextures(1, &buffer.gbm.tex_id);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, buffer.gbm.tex_id);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    if (target_tex_2d == nullptr) {
      target_tex_2d = reinterpret_cast<PFNGLEGLIMAGETARGETTEXTURE2DOESPROC>(eglGetProcAddress("glEGLImageTargetTexture2DOES"));
    }
    assert(target_tex_2d);
    target_tex_2d(GL_TEXTURE_2D, buffer.gbm.img);

    glGenFramebuffers(1, &buffer.gbm.fbo_id);
    glBindFramebuffer(GL_FRAMEBUFFER, buffer.gbm.fbo_id);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
              buffer.gbm.tex_id, 0);
    assert(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);

    return buffer;
  }

  void Display::get_edid(int adapterFD, drmModeObjectPropertiesPtr props) {
    drmModePropertyBlobPtr blob = nullptr;
	  	  uint32_t blob_id = 0;
	  int ret = 0;

	  blob_id = drm_property_get_value(&this->props.connector.at(drm::WDRM_CONNECTOR_EDID), props, 0);
    if (blob_id == 0) {
      debug("[%s] output does not have EDID\n", this->name.c_str());
      return;
    }

	  blob = drmModeGetPropertyBlob(adapterFD, blob_id);
	
    auto edid = Edid(static_cast<const uint8_t*>(blob->data), blob->length);
    drmModeFreePropertyBlob(blob);

    debug("[%s] EDID PNP ID %s, EISA ID %s, name %s, serial %s\n",
      this->name.c_str(), edid.pnp_id.data(), edid.eisa_id.data(),
      edid.monitor_name.data(), edid.serial_number.data());
      
  }

  void Display::plane_formats_populate(int adapterFD, drmModeObjectPropertiesPtr props) {


    uint32_t blob_id = drm::drm_property_get_value(&this->props.plane.at(drm::WDRM_PLANE_IN_FORMATS), props, 0);

    if(blob_id == 0) {
      debug("[%s] plane does not have IN_FORMATS\n", this->name.c_str());
		  return;
    }
    
    auto *blob = drmModeGetPropertyBlob(adapterFD, blob_id);

    auto *fmt_mod_blob = static_cast<drm_format_modifier_blob*>(blob->data);
    auto *blob_formats = formats_ptr(fmt_mod_blob);
    auto *blob_modifiers = modifiers_ptr(fmt_mod_blob);

    for (unsigned int idx = 0; idx < fmt_mod_blob->count_formats; idx++) {
      if (blob_formats[idx] != DRM_FORMAT_XRGB8888) {
        continue;
      }

      for (unsigned int idx1 = 0; idx1 < fmt_mod_blob->count_modifiers; idx1++) {
        struct drm_format_modifier *mod = &blob_modifiers[idx1];

        if ((idx < mod->offset) || (idx > mod->offset + 63)) {
          continue;
        }
        if ((mod->formats & (1 << (idx - mod->offset))) == 0U) {
          continue;
        }
        // I think this is a simple vector of .
        this->modifiers.emplace_back(mod->modifier);
      }
    }
	  drmModeFreePropertyBlob(blob);
  }
}