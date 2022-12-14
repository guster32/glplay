#pragma once
#include <memory>
#include <xf86drm.h>
#include <xf86drmMode.h>
#include <cassert>
#include <map>

namespace glplay::drm {
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
}