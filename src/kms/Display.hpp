#pragma once

#include <array>
#include <asm-generic/int-ll64.h>
#include <string>
#include <vector>

#include <xf86drm.h>
#include <xf86drmMode.h>

#include <EGL/egl.h>
#include <EGL/eglext.h>

#include <GLES3/gl3.h>
#include <GLES3/gl3ext.h>
/*
 * GLES2 exts is needed for prototypes of GL_OES_EGL_image
 * (i.e. glEGLImageTargetTexture2DOES etc.)
 */
#include <GLES2/gl2ext.h>

#include "drm_mode.h"
#include <drm_fourcc.h>
#include "../nix/nix.hpp"
#include "../drm/drm.hpp"
#include "../gbm/gbm.hpp"
#include "time.hpp"
#include "Edid.hpp"
#include "../egl/egl.hpp"

namespace glplay::kms {

  const int BUFFER_QUEUE_DEPTH = 3;

  struct Buffer {
    /*
    * true if this buffer is currently owned by KMS.
    */
    bool in_use = false;

    bool supportsFBModifiers = true;

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
    std::array<uint32_t, 4> gem_handles{};

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
      GLuint tex_id;
      GLuint fbo_id;
    } gbm{};

    unsigned int width{};
    unsigned int height{};
    std::array<unsigned int, 4> pitches{}; /* in bytes */
    std::array<unsigned int, 4> offsets{}; /* in bytes */
  };

  class Display {
    public:
      explicit Display(int adapterFD, drm::Connector &connector, drm::Crtc &crtc, drm::Plane &plane);
      void createEGLBuffers(int adapterFD, bool adapterSupportsFBModifiers, egl::EGLDevice &eglDevice, gbm::GBMDevice &gbmDevice);
      bool needs_repaint = true;
      /* Whether or not the output supports explicit fencing. */
      bool explicitFencing;
      Buffer *bufferPending;
      Buffer *bufferLast;
      
      /* Fence FD for completion of the last atomic commit. */
      int commitFenceFD = -1;
  
      /*
      * Time the last frame's commit completed from KMS, and when the
      * next frame's commit is predicted to complete.
      */
      struct timespec last_frame;
      struct timespec next_frame;

      /*
      * The frame of the animation to display.
      */
      unsigned int frame_num;
	    int64_t refreshIntervalNsec = -1;
      /* Buffers allocated by us.*/
      std::vector<Buffer> buffers;
      drm::Crtc& crtc;
      std::string name;

    private:
      drm::Plane& primary_plane;
      drm::Connector& connector;
      drm::props props;  
      void plane_formats_populate(int adapterFD, drmModeObjectPropertiesPtr props);
      void get_edid(int adapterFD, drmModeObjectPropertiesPtr props);
      auto createEGLBuffer(int adapterFD, bool adapterSupportsFBModifiers, egl::EGLDevice &eglDevice, gbm::GBMDevice &gbmDevice) -> Buffer;
      static void failOnBOCreationError(Buffer &buffer, std::array<int, 4> dma_buf_fds);
      static void buffer_egl_destroy(int adapterFD, egl::EGLDevice &eglDevice, Buffer &buffer);

      //   /* Supported format modifiers for XRGB8888. */
      std::vector<uint64_t> modifiers;
      uint32_t mode_blob_id = 0;
      drmModeModeInfo mode;

    //         /*
    //   * Time the last frame's commit completed from KMS, and when the
    //   * next frame's commit is predicted to complete.
    //   */
    //   struct timespec last_frame;
    //   struct timespec next_frame;

    //   /*
    //   * The frame of the animation to display.
    //   */
    //   unsigned int frame_num;

    //   struct {
    //     EGLConfig cfg;
    //     EGLContext ctx;
    //     GLuint gl_prog;
    //     GLuint pos_attr;
    //     GLuint col_uniform;
    //     GLuint vbo;
    //     GLuint vao;
    //     /* Whether to use big OpenGL Core Profile context or to use GLES */
    //     bool gl_core;
    //   } egl;

  };


  /*
  * The IN_FORMATS blob has two variable-length arrays at the end; one of
  * uint32_t formats, and another of the supported modifiers. To allow the
  * blob to be extended and carry more information, they carry byte offsets
  * pointing to the start of the two arrays.
  */
  inline static auto formats_ptr(drm_format_modifier_blob *blob) -> uint32_t *
  {
    return reinterpret_cast<uint32_t *>((reinterpret_cast<char *>(blob)) + blob->formats_offset);
  }

  inline static auto modifiers_ptr(struct drm_format_modifier_blob *blob) -> struct drm_format_modifier *
  {
    return reinterpret_cast<struct drm_format_modifier *>((reinterpret_cast<char *>(blob)) + blob->modifiers_offset);
  }

}

// struct output {

// 	struct {
// 		struct drm_property_info plane[WDRM_PLANE__COUNT];
// 		struct drm_property_info crtc[WDRM_CRTC__COUNT];
// 		struct drm_property_info connector[WDRM_CONNECTOR__COUNT];
// 	} props;

// 	/* Buffers allocated by us. */
// 	struct buffer *buffers[BUFFER_QUEUE_DEPTH];

// 	/*
// 	 * The buffer we've just committed to KMS, waiting for it to send the
// 	 * atomic-complete event to tell us it's started displaying; set by
// 	 * repaint_one_output and cleared by atomic_event_handler.
// 	 */
// 	struct buffer *buffer_pending;

// 	/*
// 	 * The buffer currently being displayed by KMS, having been advanced
// 	 * from buffer_pending inside atomic_event_handler, then cleared by
// 	 * atomic_event_handler when the hardware starts displaying the next
// 	 * buffer.
// 	 */
// 	struct buffer *buffer_last;

// };