#include "Resources.hpp"

namespace glplay::drm {
  //Constructor
  Resources::Resources(int fileDesc): fileDesc(fileDesc), resources(drmModeGetResources(fileDesc)) {
    if (resources == nullptr) {
			throw std::runtime_error(std::string("Unable to get resources: ") + strerror(errno));
		}
  }

  //Destructor
  Resources::~Resources() {
    if (resources != nullptr) {
      drmModeFreeResources(resources);
    }
  }

  //Copy constructor
  // Do deep copy
  Resources::Resources(const Resources& other) : fileDesc(other.fileDesc), resources(other.resources) { 
    if (resources == nullptr) {
			throw std::runtime_error(std::string("Unable to get resources: ") + strerror(errno));
		}
  }

  // Move constructor
  // Transfer ownership 
  Resources::Resources(Resources&& other) noexcept : fileDesc(other.fileDesc), resources(other.resources) {
    other.fileDesc = -1;
    other.resources = nullptr;
  }

  // Copy assignment
  // Do deep copy 
  auto Resources::operator=(const Resources& other) -> Resources& {
    if (&other == this) {
      return *this;
    }
    if(resources != nullptr) {
      drmModeFreeResources(resources);
    }
    resources = other.resources;
    fileDesc = other.fileDesc;
    return *this;
  }

  // Move assignment
  // Transfer ownership 
  auto Resources::operator=(Resources&& other) noexcept -> Resources& {
    if (&other == this) {
      return *this;
    }
    if(resources != nullptr) {
      drmModeFreeResources(resources);
    }
    resources = other.resources;
    fileDesc = other.fileDesc;
    other.fileDesc = -1;
    other.resources = nullptr;
    return *this;
  }

}
