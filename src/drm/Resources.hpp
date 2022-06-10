#pragma once

#include <string>
#include <cstring>
#include <stdexcept>

#include <xf86drmMode.h>


namespace glplay::drm {

  class Resources {
    public:
      explicit Resources(int fileDesc);
      Resources(const Resources& other); //Copy constructor
      Resources(Resources&& other) noexcept; //Move constructor
      auto operator=(const Resources& other) -> Resources&; //Copy assignment
      auto operator=(Resources&& other) noexcept -> Resources&; //Move assignment
      [[nodiscard]] auto operator->() const -> drmModeResPtr { return resources; }

      ~Resources();
    private:
      int fileDesc = -1;
      drmModeResPtr resources = nullptr;
  };

}
