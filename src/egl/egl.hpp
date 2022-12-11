#include <cassert>
#include <EGL/egl.h>
#include <EGL/eglext.h>

#if defined(HAVE_GL_CORE)
#include <GL/gl.h>
#include <GL/glext.h>
#else
#include <GLES3/gl3.h>
#include <GLES3/gl3ext.h>
/*
 * GLES2 exts is needed for prototypes of GL_OES_EGL_image
 * (i.e. glEGLImageTargetTexture2DOES etc.)
 */
#include <GLES2/gl2ext.h>
#endif

#include <gbm.h>
#include <memory>
#include <string>
#include <vector>
#include <drm_fourcc.h>
#include <cinttypes>

#include "../../third-party/gsl/gsl"

#include "../nix/nix.hpp"


namespace glplay::egl {

  /* The following is boring boilerplate GL to draw four quads. */
  static const char *vert_shader_text_gles =
    "precision highp float;\n"
    "attribute vec2 in_pos;\n"
    "void main() {\n"
    "  gl_Position = vec4(in_pos, 0.0, 1.0);\n"
    "}\n";

  static const char *frag_shader_text_gles =
    "precision highp float;\n"
    "uniform vec4 u_col;\n"
    "void main() {\n"
    "  gl_FragColor = u_col;\n"
    "}\n";

  static const char *vert_shader_text_glcore =
    "#version 330 core\n"
    "in vec2 in_pos;\n"
    "void main() {\n"
    "  gl_Position = vec4(in_pos, 0.0, 1.0);\n"
    "}\n";

  static const char *frag_shader_text_glcore =
    "#version 330 core\n"
    "uniform vec4 u_col;\n"
    "out vec4 out_color;\n"
    "void main() {\n"
    "  out_color = u_col;\n"
    "}\n";

  struct MyEGL {
		EGLConfig cfg;
		EGLContext ctx;
		GLuint gl_prog;
		GLuint pos_attr;
		GLuint col_uniform;
		GLuint vbo;
		GLuint vao;
		/* Whether to use big OpenGL Core Profile context or to use GLES */
		bool gl_core;
	};

  inline auto gl_extension_supported(const char *haystack, const char *needle) -> bool {
    size_t extlen = strlen(needle);
    const char *end = haystack + strlen(haystack);

    while (haystack < end) {
      size_t n = 0;

      /* Skip whitespaces, if any */
      if (*haystack == ' ') {
        haystack++;
        continue;
      }

      n = strcspn(haystack, " ");

      /* Compare strings */
      if (n == extlen && strncmp(needle, haystack, n) == 0)
        return true; /* Found */

      haystack += n;
    }

    /* Not found */
    return false;
  }



  inline auto device_egl_setup(gbm_device* gbm, bool fb_modifiers) -> EGLDisplay {
    EGLDisplay display = nullptr;
    // Get supported extensions without considering a display
    const char *exts = eglQueryString(EGL_NO_DISPLAY, EGL_EXTENSIONS);


    /*
    * Try to use eglGetPlatformDisplay.
    */
    if ((exts != nullptr) && (gl_extension_supported(exts, "EGL_KHR_platform_gbm") || gl_extension_supported(exts, "EGL_MESA_platform_gbm"))) {
      auto get_dpy = reinterpret_cast<PFNEGLGETPLATFORMDISPLAYEXTPROC>(eglGetProcAddress("eglGetPlatformDisplayEXT"));
      display = get_dpy(EGL_PLATFORM_GBM_KHR, gbm, nullptr);
    } else {
      // display = eglGetDisplay(gbm);
      throw std::runtime_error("couldn't create EGLDisplay from GBM device. Legacy eglGetDisplay not supported.\n");
    }

    if (display == nullptr) {
      throw std::runtime_error("couldn't create EGLDisplay from GBM device\n");
	  }

    if (eglInitialize(display, nullptr, nullptr) == 0U) {
      throw std::runtime_error("couldn't initialise EGL display\n");
    }

    exts = eglQueryString(display, EGL_EXTENSIONS);
    assert(exts);

    /*
    * dmabuf import is required, since we allocate our
    * buffers independently and import them. We require both of the KMS
    * and EGL stacks to support modifiers in order to use them, but not
    * having them is not fatal.
    */
    if (!gl_extension_supported(exts, "EGL_EXT_image_dma_buf_import")) {
      throw std::runtime_error("EGL dmabuf import not supported\n");
    }

    fb_modifiers &= gl_extension_supported(exts, "EGL_EXT_image_dma_buf_import_modifiers");
    debug("%susing format modifiers\n",(fb_modifiers) ? "" : "not ");

    /*
    * At the cost of wasted allocations, we could avoid the need for
    * surfaceless_context by allocating a scratch gbm_surface which
    * we never use, apart from having it around to make the context
    * current. But that is not implemented here.
    */
    if (!gl_extension_supported(exts, "EGL_KHR_surfaceless_context")) {
      throw std::runtime_error("EGL surfaceless context not supported");
    }

    return display;
  }

  inline auto egl_find_config(EGLDisplay display) -> EGLConfig {
    EGLConfig ret = EGL_NO_CONFIG_KHR;
    EGLint num_cfg = 0;
    EGLBoolean err = 0;

    /*
    * In order to render correctly, we need to find an EGLConfig which
    * actually corresponds to our DRM format config, else we'll get
    * channel size/order mismatches. GBM implements this by providing
    * the DRM format in the EGL_NATIVE_VISUAL_ID field of the configs.
    *
    * Unfortunately, we can't just do this the 'normal' way by passing
    * EGL_NATIVE_VISUAL_ID as a constraint in the attribs field of
    * eglChooseConfig, because eglChooseConfig is specified to ignore
    * that field, and not generate an error if you pass it.
    *
    * Instead, we loop over every available config and query its
    * NATIVE_VISUAL_ID until we find one.
    */
    err = eglGetConfigs(display, nullptr, 0, &num_cfg);
    assert(err);
    auto configs = std::vector<EGLConfig>(num_cfg);
    err = eglGetConfigs(display, configs.data(), num_cfg, &num_cfg);
    assert(err);

    for (EGLint config_idx = 0; config_idx < num_cfg; config_idx++) {
      EGLint visual;
      err = eglGetConfigAttrib(display, configs.at(config_idx),
            EGL_NATIVE_VISUAL_ID, &visual);
      assert(err);
      if (visual == DRM_FORMAT_XRGB8888) {
        ret = configs.at(config_idx);
        break;
      }
    }

    if (ret == nullptr) {
      error("no EGL config for format 0x%" PRIx32 "\n",
            DRM_FORMAT_XRGB8888);
    }

    return ret;
  }

  inline auto egl_create_context(EGLDisplay display, EGLConfig config, bool glCore) -> EGLContext {
    //struct device *device = output->device;
    const char *exts = eglQueryString(display, EGL_EXTENSIONS);
    EGLBoolean err = 0;
    EGLContext ret = nullptr;
    EGLint nattribs = 2;
    EGLint attribs[] = {
      EGL_CONTEXT_MAJOR_VERSION, 3,
      EGL_NONE,                  EGL_NONE,
      EGL_NONE,                  EGL_NONE,
      EGL_NONE,                  EGL_NONE,
      EGL_NONE,                  EGL_NONE
    };
    EGLint *attrib_version = &attribs[1];

    if (glCore) {
      attribs[nattribs++] = EGL_CONTEXT_MINOR_VERSION;
      attribs[nattribs++] = 3;
      attribs[nattribs++] = EGL_CONTEXT_OPENGL_PROFILE_MASK;
      attribs[nattribs++] = EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT;

      err = eglBindAPI(EGL_OPENGL_API);
      assert(err);
    } else {
      err = eglBindAPI(EGL_OPENGL_ES_API);
      assert(err);
    }

    /*
    * Try to give ourselves a high-priority context if we can; this may or
    * may not work, depending on the platform.
    */
    if (gl_extension_supported(exts, "EGL_IMG_context_priority")) {
      attribs[nattribs++] = EGL_CONTEXT_PRIORITY_LEVEL_IMG;
      attribs[nattribs++] = EGL_CONTEXT_PRIORITY_HIGH_IMG;

      ret = eglCreateContext(display, config,
                EGL_NO_CONTEXT, attribs);
      if (ret)
        return ret;

      /* Fall back if we can't create a high-priority context. */
      attribs[--nattribs] = EGL_NONE;
      attribs[--nattribs] = EGL_NONE;
      debug("couldn't create high-priority EGL context, falling back\n");
    }

    ret = eglCreateContext(display, config,
              EGL_NO_CONTEXT, attribs);
    if (ret)
      return ret;

    debug("couldn't create GLES3 context, falling back\n");

    if (!glCore) {
        *attrib_version = 2;
        /* As a last-ditch attempt, try an ES2 context. */
        ret = eglCreateContext(display, config,
            EGL_NO_CONTEXT, attribs);
        if (ret)
          return ret;
    }

    error("couldn't create any EGL context!\n");
    return EGL_NO_CONTEXT;
  }

  inline auto create_shader(GLuint program, const char *source, GLenum shader_type) -> GLuint {
    GLuint shader = 0;
    GLint status = 0;

    shader = glCreateShader(shader_type);
    assert(shader != 0);

    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);

    glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
    if (status == 0) {
            char log[1000];
            GLsizei len;
            glGetShaderInfoLog(shader, 1000, &len, log);
            fprintf(stderr, "Error: compiling %s: %*s\n",
                    shader_type == GL_VERTEX_SHADER ? "vertex" : "fragment",
                    len, log);
            return EGL_FALSE;
    }
	  glAttachShader(program, shader);
  	glDeleteShader(shader);

    return EGL_TRUE;
  }

  inline auto output_egl_setup(EGLDisplay display) -> MyEGL {
    MyEGL myEGL{};
    const char *exts = eglQueryString(display, EGL_EXTENSIONS);
    
    GLint status;
    if (getenv("GL_CORE")) {
      myEGL.gl_core = true;
    }

    /*
    * Explicit fencing support requires us to be able to export EGLSync
    * objects to dma_fence FDs (to give to KMS, so it can wait on GPU
    * rendering completion), and to be able to import dma_fence FDs to
    * EGLSync objects to give to GL, so it can wait on KMS completion.
    */
	  auto explicit_fencing =
		(gl_extension_supported(exts, "EGL_KHR_fence_sync") &&
		 gl_extension_supported(exts, "EGL_KHR_wait_sync") &&
		 gl_extension_supported(exts, "EGL_ANDROID_native_fence_sync"));
    debug("%susing explicit fencing\n", (explicit_fencing) ? "" : "not ");
    myEGL.cfg = egl_find_config(display);
    myEGL.ctx = egl_create_context(display, myEGL.cfg, myEGL.gl_core);
    if(myEGL.ctx == EGL_NO_CONTEXT) {
      throw std::runtime_error("Failed to create egl context");
    }
    EGLBoolean ret = eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, myEGL.ctx);
    assert(ret);
    exts = nullptr;

    /* glGetString on GL Core with GL_EXTENSIONS is an error,
    * so only do that if not using GL Core */
    if (!myEGL.gl_core) {
      exts = reinterpret_cast<const char *>(glGetString(GL_EXTENSIONS));
    }

    if (exts != nullptr) {
      if (!gl_extension_supported(exts, "GL_OES_EGL_image")) {
        error("GL_OES_EGL_image not supported\n");
        eglDestroyContext(display, myEGL.ctx);
      }

      if (explicit_fencing &&
        !gl_extension_supported(exts, "GL_OES_EGL_sync")) {
        error("GL_OES_EGL_sync not supported\n");
        eglDestroyContext(display, myEGL.ctx);
      }
    } else {
      const GLubyte *ext;
      bool found_image = false;
      bool found_sync = false;
      int num_exts = 0;

      glGetIntegerv(GL_NUM_EXTENSIONS, &num_exts);

      for (int i = 0; i < num_exts; i++) {
        ext = glGetStringi(GL_EXTENSIONS, i);
        if (strcmp(reinterpret_cast<const char *>(ext), "GL_OES_EGL_image") == 0) {
          found_image = true;
        } else if (strcmp(reinterpret_cast<const char *>(ext), "GL_OES_EGL_sync") == 0) {
          found_sync = true;
        }
      }

      if (!found_image) {
        error("GL_OES_EGL_image not supported\n");
        eglDestroyContext(display, myEGL.ctx);
      }

      if (explicit_fencing && !found_sync) {
        error("GL_OES_EGL_sync not supported\n");
        eglDestroyContext(display, myEGL.ctx);
      }
    }

    printf("using GL setup: \n"
		"   renderer '%s'\n"
		"   vendor '%s'\n"
		"   GL version '%s'\n"
		"   GLSL version '%s'\n",
		glGetString(GL_RENDERER), glGetString(GL_VENDOR),
		glGetString(GL_VERSION), glGetString(GL_SHADING_LANGUAGE_VERSION));

    myEGL.gl_prog = glCreateProgram();
    ret = create_shader(myEGL.gl_prog, myEGL.gl_core ? vert_shader_text_glcore : vert_shader_text_gles, GL_VERTEX_SHADER);
    assert(ret);
    ret = create_shader(myEGL.gl_prog, myEGL.gl_core ? frag_shader_text_glcore : frag_shader_text_gles, GL_FRAGMENT_SHADER);
    assert(ret);

    myEGL.pos_attr = 0;
    glBindAttribLocation(myEGL.gl_prog, myEGL.pos_attr, "in_pos");

    glLinkProgram(myEGL.gl_prog);
    glGetProgramiv(myEGL.gl_prog, GL_LINK_STATUS, &status);
    if (!status) {
      char log[1000];
      GLsizei len;
      glGetProgramInfoLog(myEGL.gl_prog, 1000, &len, log);
      error("Error: linking GLSL program: %*s\n", len, log);
      glDeleteProgram(myEGL.gl_prog);
    }
    assert(status);

    myEGL.col_uniform = glGetUniformLocation(myEGL.gl_prog, "u_col");
    glUseProgram(myEGL.gl_prog);

    glGenBuffers(1, &myEGL.vbo);
    glBindBuffer(GL_ARRAY_BUFFER, myEGL.vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(GLfloat) * 8, nullptr, GL_DYNAMIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    glGenVertexArrays(1, &myEGL.vao);
    glBindVertexArray(myEGL.vao);

    glBindBuffer(GL_ARRAY_BUFFER, myEGL.vbo);

    glVertexAttribPointer(myEGL.pos_attr, 2, GL_FLOAT, GL_FALSE, 0, (char*)nullptr);
    glEnableVertexAttribArray(0);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    return myEGL;
  }
}