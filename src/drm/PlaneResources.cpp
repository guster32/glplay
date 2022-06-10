#include "PlaneResources.hpp"

namespace glplay::drm {
  //Constructor
  PlaneResources::PlaneResources(int fileDesc): fileDesc(fileDesc), resources(drmModeGetPlaneResources(fileDesc)) {
    if (resources == nullptr) {
			throw std::runtime_error(std::string("Unable to get plane resources: ") + strerror(errno));
		}
  }

  //Destructor
  PlaneResources::~PlaneResources() {
    if (resources != nullptr) {
      drmModeFreePlaneResources(resources);
    }
  }

  //Copy constructor
  // Do deep copy
  PlaneResources::PlaneResources(const PlaneResources& other) : fileDesc(other.fileDesc), resources(other.resources) { 
    if (resources == nullptr) {
			throw std::runtime_error(std::string("Unable to get plane resources: ") + strerror(errno));
		}
  }

  // Move constructor
  // Transfer ownership 
  PlaneResources::PlaneResources(PlaneResources&& other) noexcept : fileDesc(other.fileDesc), resources(other.resources) {
    other.fileDesc = -1;
    other.resources = nullptr;
  }

  // Copy assignment
  // Do deep copy 
  auto PlaneResources::operator=(const PlaneResources& other) -> PlaneResources& {
    if (&other == this) {
      return *this;
    }
    if(resources != nullptr) {
      drmModeFreePlaneResources(resources);
    }
    resources = other.resources;
    fileDesc = other.fileDesc;
    return *this;
  }

  // Move assignment
  // Transfer ownership 
  auto PlaneResources::operator=(PlaneResources&& other) noexcept -> PlaneResources& {
    if (&other == this) {
      return *this;
    }
    if(resources != nullptr) {
      drmModeFreePlaneResources(resources);
    }
    resources = other.resources;
    fileDesc = other.fileDesc;
    other.resources = nullptr;
    other.fileDesc = -1;
    return *this;
  }

}
