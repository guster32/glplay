#include <cassert>
#include <EGL/egl.h>
#include <EGL/eglext.h>

#include <gbm.h>
#include <memory>
#include <string>

#include "../../third-party/gsl/gsl"

#include "../nix/nix.hpp"


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
      if (n == extlen && strncmp(needle, haystack, n) == 0)
        return true; /* Found */

      haystack += n;
    }

    /* Not found */
    return false;
  }



  inline auto device_egl_setup(gbm_device* gbm, bool fb_modifiers) -> EGLDisplay {
    EGLDisplay display;
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
}