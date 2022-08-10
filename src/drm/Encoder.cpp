#include "Encoder.hpp"

namespace glplay::drm {
  //Constructor
  Encoder::Encoder(int fileDesc, uint32_t encoderId): fileDesc(fileDesc), encoderId(encoderId), encoder(drmModeGetEncoder(fileDesc, encoderId)) {
    if (encoder == nullptr) {
			throw std::runtime_error(std::string("Unable to get encoder: ") + strerror(errno));
		}
  }

  //Destructor
  Encoder::~Encoder() {
    if (encoder != nullptr) {
      drmModeFreeEncoder(encoder);
    }
  }

  //Copy constructor
  // Do deep copy
  Encoder::Encoder(const Encoder& other): fileDesc(other.fileDesc), encoderId(other.encoderId), encoder(other.encoder) { 
    if (encoder == nullptr) {
			throw std::runtime_error(std::string("Unable to get encoder: ") + strerror(errno));
		}
  }

  // Move constructor
  // Transfer ownership 
  Encoder::Encoder(Encoder&& other) noexcept : fileDesc(other.fileDesc), encoderId(other.encoderId), encoder(other.encoder) {
    other.fileDesc = -1;
    other.encoderId = -1;
    other.encoder = nullptr;
  }

  // Copy assignment
  // Do deep copy 
  auto Encoder::operator=(const Encoder& other) -> Encoder& {
    if (&other == this) {
      return *this;
    }
    if(encoder != nullptr) {
      drmModeFreeEncoder(encoder);
    }
    fileDesc = other.fileDesc;
    encoderId = other.encoderId;
    encoder = other.encoder;
    return *this;
  }

  // Move assignment
  // Transfer ownership 
  auto Encoder::operator=(Encoder&& other) noexcept -> Encoder& {
    if (&other == this) {
      return *this;
    }
    if(encoder != nullptr) {
      drmModeFreeEncoder(encoder);
    }
    fileDesc = other.fileDesc;
    encoderId = other.encoderId;
    encoder = other.encoder;

    other.fileDesc = -1;
    other.encoderId = -1;
    other.encoder = nullptr;
    return *this;
  }

  auto Encoder::hasCrtc() -> bool {
    return encoder->encoder_id != 0;
  }


}
