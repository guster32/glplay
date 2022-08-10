#pragma once

#include <string>
#include <cstring>
#include <stdexcept>

#include <xf86drmMode.h>

namespace glplay::drm {

  class Crtc {
    public:
      explicit Crtc(int fileDesc, uint32_t crtcId);
      Crtc(const Crtc& other); //Copy constructor
      Crtc(Crtc&& other) noexcept; //Move constructor
      auto operator=(const Crtc& other) -> Crtc&; //Copy assignment
      auto operator=(Crtc&& other) noexcept -> Crtc&; //Move assignment
      [[nodiscard]] auto operator->() const -> drmModeCrtcPtr { return crtc; }
      auto isActive() -> bool;

      ~Crtc();
    private:
      int fileDesc = -1;
      int crtcId = -1;
      drmModeCrtcPtr crtc = nullptr;
  };

}