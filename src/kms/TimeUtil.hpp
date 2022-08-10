#pragma once

#include <cstdint>
#include <ctime>
#include <stdexcept>


namespace glplay::kms {
  static inline auto millihzToNsec(uint32_t mhz) -> int64_t {
    if(mhz < 1) {
      throw std::runtime_error("mhz must be greater than 0");
    }
    return 1000000000000LL / mhz;
  }
}
