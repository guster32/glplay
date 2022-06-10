#include <string>
#include <cstring>
#include <stdexcept>

#include <xf86drmMode.h>

namespace glplay::drm {

  class Plane {
    public:
      explicit Plane(int fileDesc, uint32_t planeId);
      Plane(const Plane& other); //Copy constructor
      Plane(Plane&& other) noexcept; //Move constructor
      auto operator=(const Plane& other) -> Plane&; //Copy assignment
      auto operator=(Plane&& other) noexcept -> Plane&; //Move assignment
      [[nodiscard]] auto operator->() const -> drmModePlanePtr { return plane; }

      ~Plane();
    private:
      int fileDesc = -1;
      uint32_t planeId = -1;
      drmModePlanePtr plane = nullptr;
  };

}