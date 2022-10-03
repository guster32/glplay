#pragma once
#pragma once

#pragma once
#include <cassert>
#include <memory>
#include <string>

#include <gbm.h>

#include "../../third-party/gsl/gsl"
#include "../nix/nix.hpp"

namespace glplay::gbm {
  
  //GBMDevice SmartPointer
	using GBMDevice = std::shared_ptr<gbm_device>;
	struct GBMDeviceDeleter {
		void operator()(gbm_device* handle) {
			if(handle != nullptr) {
				gbm_device_destroy(handle);
			}
		}
	};
	inline auto make_gbm_ptr(int fileDesc) -> GBMDevice {
			auto *handle = gbm_create_device(fileDesc);
			assert(handle != nullptr);
			return {handle, GBMDeviceDeleter()};
	}
}

