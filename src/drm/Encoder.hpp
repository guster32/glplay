#pragma once
#include <memory>
#include <xf86drm.h>
#include <xf86drmMode.h>
#include <cassert>

namespace glplay::drm {
	//Encoder SmartPointer
	using Encoder = std::shared_ptr<drmModeEncoder>;
	struct EncoderDeleter {
		void operator()(drmModeEncoderPtr handle) {
			if(handle != nullptr) {
				drmModeFreeEncoder(handle);
			}
		}
	};
	inline auto make_encoder_ptr(int fileDesc, uint32_t encoderId) -> Encoder {
		auto *handle = drmModeGetEncoder(fileDesc, encoderId);
		assert(handle != nullptr);
		return {handle, EncoderDeleter()};
	};
}