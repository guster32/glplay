#pragma once

#include <string>
#include <cstring>
#include <stdexcept>

#include <xf86drmMode.h>

namespace glplay::drm {

  class PlaneResources {
    public:
      explicit PlaneResources(int fileDesc);
      PlaneResources(const PlaneResources& other); //Copy constructor
      PlaneResources(PlaneResources&& other) noexcept; //Move constructor
      auto operator=(const PlaneResources& other) -> PlaneResources&; //Copy assignment
      auto operator=(PlaneResources&& other) noexcept -> PlaneResources&; //Move assignment
      [[nodiscard]] auto operator->() const -> drmModePlaneResPtr { return resources; }

      ~PlaneResources();
    private:
      int fileDesc = -1;
      drmModePlaneResPtr resources = nullptr;
  };

}
