#include "Plane.hpp"

namespace glplay::drm {
  //Constructor
  Plane::Plane(int fileDesc, uint32_t planeId): fileDesc(fileDesc), planeId(planeId), plane(drmModeGetPlane(fileDesc, planeId)) {
    if (plane == nullptr) {
			throw std::runtime_error(std::string("Unable to get plane: ") + strerror(errno));
		}
  }

  //Destructor
  Plane::~Plane() {
    if (plane != nullptr) {
      drmModeFreePlane(plane);
    }
  }

  //Copy constructor
  // Do deep copy
  Plane::Plane(const Plane& other): fileDesc(other.fileDesc), planeId(other.planeId), plane(other.plane) { 
    if (plane == nullptr) {
			throw std::runtime_error(std::string("Unable to get plane: ") + strerror(errno));
		}
  }

  // Move constructor
  // Transfer ownership 
  Plane::Plane(Plane&& other) noexcept : fileDesc(other.fileDesc), planeId(other.planeId), plane(other.plane) {
    other.fileDesc = -1;
    other.planeId = 0;
    other.plane = nullptr;
  }

  // Copy assignment
  // Do deep copy 
  auto Plane::operator=(const Plane& other) -> Plane& {
    if (&other == this) {
      return *this;
    }
    if(plane != nullptr) {
      drmModeFreePlane(plane);
    }
    fileDesc = other.fileDesc;
    planeId = other.planeId;
    plane = other.plane;
    return *this;
  }

  // Move assignment
  // Transfer ownership 
  auto Plane::operator=(Plane&& other) noexcept -> Plane& {
    if (&other == this) {
      return *this;
    }
    if(plane != nullptr) {
      drmModeFreePlane(plane);
    }
    fileDesc = other.fileDesc;
    planeId = other.planeId;
    plane = other.plane;

    other.fileDesc = -1;
    other.planeId = 0;
    other.plane = nullptr;
    return *this;
  }

}
