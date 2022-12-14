#pragma once
#include <memory>
#include <xf86drm.h>
#include <xf86drmMode.h>
#include <cassert>

namespace glplay::drm {
	//Crtc SmartPointer
	using Crtc = std::shared_ptr<drmModeCrtc>;
	struct CrtcDeleter {
		void operator()(drmModeCrtcPtr handle) {
			if(handle != nullptr) {
				drmModeFreeCrtc(handle);
			}
		}
	};
	inline auto make_crtc_ptr(int fileDesc, uint32_t crtcId) -> Crtc {
		auto *handle = drmModeGetCrtc(fileDesc, crtcId);
		assert(handle != nullptr);
		return {handle, CrtcDeleter()};
	};
}