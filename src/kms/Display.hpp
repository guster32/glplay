#pragma once

#include <array>
#include <asm-generic/int-ll64.h>
#include <string>
#include <vector>

#include <xf86drm.h>
#include <xf86drmMode.h>

#include <EGL/egl.h>
#include <EGL/eglext.h>

#if defined(HAVE_GL_CORE)
#include <GL/gl.h>
#include <GL/glext.h>
#else
#include <GLES3/gl3.h>
#include <GLES3/gl3ext.h>
#endif
#include "drm_mode.h"
#include <drm_fourcc.h>
#include "../nix/nix.hpp"
#include "../drm/drm.hpp"
#include "time.hpp"
#include "Edid.hpp"

namespace glplay::kms {


  class Display {
    public:
      explicit Display(int adapterFD, drm::Connector &connector, drm::Crtc &crtc, drm::Plane &plane);
    private:
      std::string name;
      bool needs_repaint = true;
      drm::Plane& primary_plane;
      drm::Crtc& crtc;
      drm::Connector& connector;
      drm::props props;  
      void plane_formats_populate(int adapterFD, drmModeObjectPropertiesPtr props);
      void get_edid(int adapterFD, drmModeObjectPropertiesPtr props);
      
      //   /* Supported format modifiers for XRGB8888. */
      std::vector<__u64> modifiers;
      uint32_t mode_blob_id = 0;
      drmModeModeInfo mode;
	    int64_t refreshIntervalNsec = -1;
      /* Whether or not the output supports explicit fencing. */
      bool explicitFencing;
      /* Fence FD for completion of the last atomic commit. */
      int commitFenceFD = -1;

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