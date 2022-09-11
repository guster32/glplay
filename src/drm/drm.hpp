#pragma once

#include <cstdint>
#include <memory>
#include <xf86drm.h>
#include <xf86drmMode.h>
#include <cassert>

#include <string>
#include <vector>
#include <map>

#include "../../third-party/gsl/gsl"
#include "PropertyInfo.hpp"

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


	//Connector SmartPointer
	using Connector = std::shared_ptr<drmModeConnector>;
	struct ConnectorDeleter {
		void operator()(drmModeConnectorPtr handle) {
			if(handle != nullptr) {
				drmModeFreeConnector(handle);
			}
		}
	};
	inline auto make_connetor_ptr(int fileDesc, uint32_t connectorId) -> Connector {
		auto *handle = drmModeGetConnector(fileDesc, connectorId);
		assert(handle != nullptr);
		return {handle, ConnectorDeleter()};
	}

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

	
  
	static const std::map<uint32_t, std::string> connectorTypes { 
		{DRM_MODE_CONNECTOR_Unknown, "Unknown"},
		{DRM_MODE_CONNECTOR_VGA, "VGA"},
		{DRM_MODE_CONNECTOR_DVII, "DVI-I"},
		{DRM_MODE_CONNECTOR_DVID, "DVI-D"},
		{DRM_MODE_CONNECTOR_DVIA, "DVI-A"},
		{DRM_MODE_CONNECTOR_Composite, "Composite"},
		{DRM_MODE_CONNECTOR_SVIDEO, "SVIDEO"},
		{DRM_MODE_CONNECTOR_LVDS, "LVDS"},
		{DRM_MODE_CONNECTOR_Component, "Component"},
		{DRM_MODE_CONNECTOR_9PinDIN, "DIN"},
		{DRM_MODE_CONNECTOR_DisplayPort, "DP"},
		{DRM_MODE_CONNECTOR_HDMIA, "HDMI-A"},
		{DRM_MODE_CONNECTOR_HDMIB, "HDMI-B"},
		{DRM_MODE_CONNECTOR_TV, "TV"},
		{DRM_MODE_CONNECTOR_eDP, "eDP"},
		#ifdef DRM_MODE_CONNECTOR_DSI
		{DRM_MODE_CONNECTOR_VIRTUAL, "Virtual"},
		{DRM_MODE_CONNECTOR_DSI, "DSI"},
		#endif
		#ifdef DRM_MODE_CONNECTOR_DPI
		{DRM_MODE_CONNECTOR_DPI, "DPI"},
		#endif
		#ifdef DRM_MODE_CONNECTOR_WRITEBACK
		{DRM_MODE_CONNECTOR_WRITEBACK, "Writeback"}
		#endif
	};

	static void drm_property_info_populate(int adapterFD,
		const std::vector <drm_property_info> &src,
		std::vector<drm_property_info>& info,
		unsigned int num_infos,
		drmModeObjectProperties *props) {
		
		for (int i = 0; i < num_infos; i++) {
			drm_property_info inf;
			inf.name = src[i].name;
			inf.prop_id = 0;
			inf.num_enum_values = src[i].num_enum_values;

			if(inf.num_enum_values == 0) {
				info.push_back(inf);
				continue;
			}
			
			for (int j = 0; j < inf.num_enum_values; j++) {
				drm_property_enum_info enum_inf{};
				enum_inf.name = src[i].enum_values[j].name;
				enum_inf.valid = false;
				inf.enum_values.push_back(enum_inf);
			}
			info.push_back(inf);
		}

		for(int i = 0; i < props->count_props; i++) {
			auto *prop = drmModeGetProperty(adapterFD, props->props[i]);
			if (prop == nullptr) { continue; }

			int j = 0;
			for(j = 0; j < num_infos; j++) {
				if(strcmp(prop->name, info[j].name) == 0) { break; }
			}

			if(j == num_infos) {
				drmModeFreeProperty(prop);
				continue;
			}
			info[j].prop_id = props->props[i];

			for(int k = 0; k < info[j].num_enum_values; k++) {
				int l = 0;
				for(l = 0; l < prop->count_enums; l++) {
					if(strcmp(prop->enums[l].name, info[j].enum_values[k].name) == 0) { break; }
				}
				
				if(l == prop->count_enums) { continue; }

				info[j].enum_values[k].valid = true;
				info[j].enum_values[k].value = prop->enums[l].value;
			}
			drmModeFreeProperty(prop);
		}
	}

	inline auto mode_blob_create(int adapterFD, drmModeModeInfo *mode) -> uint32_t {
		uint32_t ret = 0;
		int err = 0;
		err = drmModeCreatePropertyBlob(adapterFD, mode, sizeof(*mode), &ret);
		if(err < 0) {
			throw std::runtime_error("Failed to create mode blob");
		}
		return ret;
	}

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

	/**
	* Get the current value of a KMS property
	*
	* Given a drmModeObjectGetProperties return, as well as the drm_property_info
	* for the target property, return the current value of that property,
	* with an optional default. If the property is a KMS enum type, the return
	* value will be translated into the appropriate internal enum.
	*
	* If the property is not present, the default value will be returned.
	*
	* @param info Internal structure for property to look up
	* @param props Raw KMS properties for the target object
	* @param def Value to return if property is not found
	*/
	inline static auto drm_property_get_value(struct drm_property_info *info, const drmModeObjectProperties *props, uint64_t def) -> uint64_t
	{

		if (info->prop_id == 0) {
			return def;
		}

		for (unsigned int i = 0; i < props->count_props; i++) {
			
			if (props->props[i] != info->prop_id) {
				continue;
			}

			/* Simple (non-enum) types can return the value directly */
			if (info->num_enum_values == 0) {
				return props->prop_values[i];
			}

			/* Map from raw value to enum value */
			for (unsigned int j = 0; j < info->num_enum_values; j++) {
				if (!info->enum_values[j].valid) {
					continue;
				}
				if (info->enum_values[j].value != props->prop_values[i]) {
					continue;
				}

				return j;
			}

			/* We don't have a mapping for this enum; return default. */
			break;
		}

		return def;
	}
}
