#include "Crtc.hpp"

namespace glplay::drm {
  //Constructor
  Crtc::Crtc(int fileDesc, uint32_t crtcId): fileDesc(fileDesc), crtcId(crtcId), crtc(drmModeGetCrtc(fileDesc, crtcId)) {
    if (crtc == nullptr) {
			throw std::runtime_error(std::string("Unable to get crtc: ") + strerror(errno));
		}
  }

  //Destructor
  Crtc::~Crtc() {
    if (crtc != nullptr) {
      drmModeFreeCrtc(crtc);
    }
  }

  //Copy constructor
  // Do deep copy
  Crtc::Crtc(const Crtc& other): fileDesc(other.fileDesc), crtcId(other.crtcId), crtc(other.crtc) { 
    if (crtc == nullptr) {
			throw std::runtime_error(std::string("Unable to get crtc: ") + strerror(errno));
		}
  }

  // Move constructor
  // Transfer ownership 
  Crtc::Crtc(Crtc&& other) noexcept : fileDesc(other.fileDesc), crtcId(other.crtcId), crtc(other.crtc) {
    other.fileDesc = -1;
    other.crtcId = -1;
    other.crtc = nullptr;
  }

  // Copy assignment
  // Do deep copy 
  auto Crtc::operator=(const Crtc& other) -> Crtc& {
    if (&other == this) {
      return *this;
    }
    if(crtc != nullptr) {
      drmModeFreeCrtc(crtc);
    }
    fileDesc = other.fileDesc;
    crtcId = other.crtcId;
    crtc = other.crtc;
    return *this;
  }

  // Move assignment
  // Transfer ownership 
  auto Crtc::operator=(Crtc&& other) noexcept -> Crtc& {
    if (&other == this) {
      return *this;
    }
    if(crtc != nullptr) {
      drmModeFreeCrtc(crtc);
    }
    fileDesc = other.fileDesc;
    crtcId = other.crtcId;
    crtc = other.crtc;

    other.fileDesc = -1;
    other.crtcId = -1;
    other.crtc = nullptr;
    return *this;
  }

  auto Crtc::isActive() -> bool {
    return crtc->buffer_id != 0;
  }

}
