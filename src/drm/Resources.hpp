#pragma once
#include <memory>
#include <xf86drm.h>
#include <xf86drmMode.h>
#include <cassert>

namespace glplay::drm {
	//Resources SmartPointer
	using Resources = std::shared_ptr<drmModeRes>;
	struct ResourcesDeleter {
		void operator()(drmModeResPtr handle) {
			if(handle != nullptr) {
				drmModeFreeResources(handle);
			}
		}
	};
	inline auto make_resources_ptr(int fileDesc) -> Resources {
		auto *handle = drmModeGetResources(fileDesc);
		assert(handle != nullptr);
		return {handle, ResourcesDeleter()};
	};
}