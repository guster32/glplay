#pragma once
#include <memory>
#include <xf86drm.h>
#include <xf86drmMode.h>
#include <cassert>

namespace glplay::drm {
  //Plane SmartPointer
	using Plane = std::shared_ptr<drmModePlane>;
	struct PlaneDeleter {
		void operator()(drmModePlanePtr handle) {
			if(handle != nullptr) {
				drmModeFreePlane(handle);
			}
		}
	};
	inline auto make_plane_ptr(int fileDesc, uint32_t planeId) -> Plane {
			auto *handle = drmModeGetPlane(fileDesc, planeId);
			assert(handle != nullptr);
			return {handle, PlaneDeleter()};
	} 
}