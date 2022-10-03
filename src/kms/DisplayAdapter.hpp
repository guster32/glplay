#pragma once

#include "drm.h"
#include <EGL/egl.h>
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
    private:
      EGLDisplay eglDPY;
      gbm::GBMDevice buffer;
      nix::FileDescriptor adapterFD;
      std::vector<drm::Plane> planes;
      std::vector<Display> displays;
      bool supportsFBModifiers = false;
      auto findEncoderForConnector(drm::Resources &resources, drm::Connector &connector) -> drm::Encoder;
      auto findCrtcForEncoder(drm::Resources &resources, drm::Encoder &encoder) -> drm::Crtc;
      auto findPrimaryPlaneForCrtc(drm::Crtc &crtc) -> drm::Plane;
  };

}