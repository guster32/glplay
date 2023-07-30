#pragma once

#include <cassert>
#include <EGL/egl.h>
#include <EGL/eglext.h>

#ifdef GLES3
#include <GLES3/gl3.h>
#include <GLES3/gl3ext.h>
#elif GLES2
#include <GLES2/gl2.h>
#endif
/*
 * GLES2 exts is needed for prototypes of GL_OES_EGL_image
 * (i.e. glEGLImageTargetTexture2DOES etc.)
 */
#include <GLES2/gl2ext.h>

#include <string>
#include <vector>
#include <drm_fourcc.h>
#include <cinttypes>

#include "../../third-party/gsl/gsl"
#include "../nix/nix.hpp"
#include "utils.hpp"

namespace glplay::egl {

  class EGLDevice {

    public:
      explicit EGLDevice(gbm::GBMDevice &gbmDevice);
      ~EGLDevice();
      EGLDisplay egl_dpy;
      EGLContext ctx;
		  GLuint col_uniform;
		  GLuint vbo;
		  GLuint vao;
      GLuint gl_prog;
      GLuint pos_attr;
      bool explicit_fencing;
  
    private:
      bool fb_modifiers;
      
		  EGLConfig cfg;
		  /* Whether to use big OpenGL Core Profile context or to use GLES */
		  bool gl_core;

      static auto initializeDisplay(gbm::GBMDevice &gbmDevice) -> EGLDisplay;
      static auto initializeConfig(EGLDisplay display) -> EGLConfig;
      static auto initializeContext(EGLDisplay display, EGLConfig config, bool glCore) -> EGLContext;
      static auto initializeShader(GLuint program, const char *source, GLenum shader_type) -> GLuint;
	};
}