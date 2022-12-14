#pragma once
#include <memory>
#include <xf86drm.h>
#include <xf86drmMode.h>
#include <cassert>

namespace glplay::drm {
	//PlaneResources SmartPointer
	using PlaneResources = std::shared_ptr<drmModePlaneRes>;
	struct PlaneResourcesDeleter {
		void operator()(drmModePlaneResPtr handle) {
			if(handle != nullptr) {
				drmModeFreePlaneResources(handle);
			}
		}
	};
	inline auto make_plane_resources_ptr(int fileDesc) -> PlaneResources {
		auto *handle = drmModeGetPlaneResources(fileDesc);
		assert(handle != nullptr);
		return {handle, PlaneResourcesDeleter()};
	};
}