#pragma once

#include <xf86drm.h>
#include <xf86drmMode.h>

#include <string>
#include <vector>

#include "../../third-party/gsl/gsl"

#include "Resources.hpp"
#include "PlaneResources.hpp"
#include "Encoder.hpp"
#include "Plane.hpp"
#include "Connector.hpp"
#include "Crtc.hpp"


namespace glplay::drm {
  
	/*
	* Enumerates the file descriptor paths for all PRIMARY nodes (aka. /dev/dri/card0) for KMS/modesetting
	*/
	inline auto getDevicePaths() -> std::vector<std::string> {
		
		std::vector<std::string> vect;
		int deviceCount = 0;
		deviceCount = drmGetDevices2(0, nullptr, 0);
		auto devices = gsl::unique_ptr<drmDevicePtr>(new drmDevicePtr[deviceCount]);
		deviceCount = drmGetDevices2(0, devices.get(), deviceCount);
		for (int i = 0; i < deviceCount; i++) {
			drmDevicePtr device = devices.get()[i];
			/*
			* We need a PRIMARY node (aka. /dev/dri/card0) for KMS/modesetting. 
			* Render nodes are only used for GPU rendering
			*/
			if ((device->available_nodes & (1U << DRM_NODE_PRIMARY)) == 0U){
				continue;
			}
			vect.emplace_back(std::string(device->nodes[DRM_NODE_PRIMARY]));
		}
		drmFreeDevices(devices.get(), deviceCount);
		return vect;
	}

}
