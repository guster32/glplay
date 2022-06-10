#pragma once

#include "drm.h"
#include <xf86drm.h>
#include <xf86drmMode.h>
#include <fcntl.h>

#include <vector>

#include "../drm/drm.hpp"
#include "../nix/nix.hpp"

namespace glplay::kms {
  
  class DisplayAdapter {
    
    public:
      explicit DisplayAdapter(std::string &path);
    private:
      nix::FileDescriptor adapterFD;
      std::vector<drm::Plane> planes;
      bool supportsFBModifiers = false;
  };

}