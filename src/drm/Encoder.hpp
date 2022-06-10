#include <string>
#include <cstring>
#include <stdexcept>

#include <xf86drmMode.h>

namespace glplay::drm {

  class Encoder {
    public:
      explicit Encoder(int fileDesc, int encoderId);
      Encoder(const Encoder& other); //Copy constructor
      Encoder(Encoder&& other) noexcept; //Move constructor
      auto operator=(const Encoder& other) -> Encoder&; //Copy assignment
      auto operator=(Encoder&& other) noexcept -> Encoder&; //Move assignment
      [[nodiscard]] auto operator->() const -> drmModeEncoderPtr { return encoder; }

      ~Encoder();
    private:
      int fileDesc = -1;
      int encoderId = -1;
      drmModeEncoderPtr encoder = nullptr;
  };

}