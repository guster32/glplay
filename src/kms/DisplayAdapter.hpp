#pragma once

#include "drm.h"
#include <cstddef>
#include <xf86drm.h>
#include <xf86drmMode.h>
#include <fcntl.h>

#include <cstdio>
#include <iostream>
#include <vector>

#include "Display.hpp"
#include "../drm/drm.hpp"
#include "../nix/nix.hpp"
#include "../gbm/gbm.hpp"
#include "../egl/egl.hpp"

namespace glplay::kms {
  
  class DisplayAdapter {
    
    public:
      explicit DisplayAdapter(std::string &path);
      [[nodiscard]] auto getAdapterFD() { return adapterFD.fileDescriptor(); }
      nix::FileDescriptor adapterFD;
      std::vector<Display> displays;
      gbm::GBMDevice gbmDevice;
      egl::EGLDevice eglDevice;
  };

}