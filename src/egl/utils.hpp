#pragma once

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

#include <string>
#include <vector>
#include <drm_fourcc.h>
#include <cinttypes>

#include "../../third-party/gsl/gsl"

#include "../nix/nix.hpp"
#include "../gbm/gbm.hpp"

namespace glplay::egl {
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
      if (n == extlen && strncmp(needle, haystack, n) == 0) {
        return true; /* Found */
      }

      haystack += n;
    }

    /* Not found */
    return false;
  }

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
}