#pragma once

#include <string>
#include <cstring>
#include <stdexcept>

#include <xf86drmMode.h>

namespace glplay::drm {

  class Encoder {
    public:
      explicit Encoder(int fileDesc, uint32_t encoderId);
      Encoder(const Encoder& other); //Copy constructor
      Encoder(Encoder&& other) noexcept; //Move constructor
      auto operator=(const Encoder& other) -> Encoder&; //Copy assignment
      auto operator=(Encoder&& other) noexcept -> Encoder&; //Move assignment
      [[nodiscard]] auto operator->() const -> drmModeEncoderPtr { return encoder; }
      auto hasCrtc() -> bool;

      ~Encoder();
    private:
      int fileDesc = -1;
      uint32_t encoderId = -1;
      drmModeEncoderPtr encoder = nullptr;
  };

}