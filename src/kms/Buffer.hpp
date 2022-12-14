#pragma once

#include <EGL/egl.h>
#include <GLES3/gl3.h>
#include <array>
#include <cstdint>
#include <gbm.h>
#include "Display.hpp"

namespace glplay::kms {

  class Buffer {
    private:
      /*
      * true if this buffer is currently owned by KMS.
      */
      bool in_use = true;

      	/*
        * The GEM handle for this buffer, returned from the dumb-buffer
        * creation ioctl. GEM names are also returned from
        * drmModePrimeFDToHandle when importing dmabufs directly, however
        * this has huge caveats and you almost certainly shouldn't do it
        * directly.
        *
        * GEM handles (or 'names') represents a dumb bag of bits: in order to
        * display the buffer we've created, we need to create a framebuffer,
        * which annotates our GEM buffer with additional metadata.
        *
        * 0 is always an invalid GEM handle.
        */
        // std::array<uint32_t, 4> gem_handles;

        /*
        * Framebuffers wrap GEM buffers with additional metadata such as the
        * image dimensions. It is this framebuffer ID which is passed to KMS
        * to display.
        *
        * 0 is always an invalid framebuffer id.
        */
        uint32_t fb_id{};

        /*
        * dma_fence FD for completion of the rendering to this buffer.
        */
        int render_fence_fd{};

        /*
        * dma_fence FD for completion of the last KMS commit this buffer
        * was used in.
        */
        int kms_fence_fd{};

        /*
        * The format and modifier together describe how the image is laid
        * out in memory; both are canonically described in drm_fourcc.h.
        *
        * The format contains information on how each pixel is laid out:
        * the type, ordering, and width of each colour channel within the
        * pixel. The modifier contains information on how pixels are laid
        * out within the buffer, e.g. whether they are purely linear,
        * tiled, compressed, etc.
        *
        * Each plane has a property named 'IN_FORMATS' which describes
        * the format + modifier combinations it will accept; the framebuffer
        * must be allocated accordingly.
        *
        * An example of parsing 'IN_FORMATS' to create an array of formats,
        * each containing an array of modifiers, can be found in libweston's
        * compositor-drm.c.
        *
        * 0 is always an invalid format ID. However, a 0 modifier always means
        * that the image has a linear layout; DRM_FORMAT_MOD_INVALID is the
        * invalid modifier, which is used to signify that the user does not
        * know or care about the modifier being used.
        */
        uint32_t format{};
        uint64_t modifier{};

        /* Parameters for our memory-mapped image. */
        // struct {
        //   uint32_t *mem;
        //   unsigned int size;
        // } dumb;

        struct {
          struct gbm_bo *bo;
          EGLImage img;
          GLuint text_id;
          GLuint fbo_id;
        } gbm;

        unsigned int width{};
        unsigned int height{};
        unsigned int pitches[4]{}; /* in bytes */
        unsigned int offsets[4]{}; /* in bytes */

  };

  /*
  * Allocates a buffer and makes it usable for rendering with EGL/GL. We achieve
  * this by allocating each individual buffer with GBM, importing it into EGL
  * as an EGLImage, binding the EGLImage to a texture unit, then finally creating
  * a FBO from that texture unit so we can render into it.
  */
  inline auto buffer_egl_create(gbm_device* gbm, Display *display) -> Buffer {
    
  }

  inline auto buffer_create(gbm_device* gbm, Display *display) -> Buffer {
    Buffer ret;
    std::array<uint64_t, 4> modifier = {0, };
    // Here is where we would create a vk_buffer.
    ret = buffer_egl_create(gbm, display);
  }

}