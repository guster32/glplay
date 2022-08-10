#pragma once

#include <string>
#include <cstring>
#include <array>
#include <vector>

namespace glplay::drm {

  struct drm_property_enum_info {
    const char* name;
    bool valid;
    uint64_t value;
  };

  struct drm_property_info {
    const char* name;
    uint32_t prop_id;
    unsigned int num_enum_values;
    std::vector<drm_property_enum_info> enum_values;
  };

  /**
  * Possible values for the WDRM_PLANE_TYPE property.
  */
  enum wdrm_plane_type {
    WDRM_PLANE_TYPE_PRIMARY = 0,
    WDRM_PLANE_TYPE_CURSOR,
    WDRM_PLANE_TYPE_OVERLAY,
    WDRM_PLANE_TYPE_COUNT
  };

  enum wdrm_dpms_state {
    WDRM_DPMS_STATE_OFF = 0,
    WDRM_DPMS_STATE_ON,
    WDRM_DPMS_STATE_STANDBY, /* unused */
    WDRM_DPMS_STATE_SUSPEND, /* unused */
    WDRM_DPMS_STATE_COUNT
  };

  static const std::vector<drm_property_enum_info> dpms_state_enums {
    { .name = "Off" },
    { .name = "On" },
    { .name = "Standby" },
    { .name = "Suspend" },
  };

  static const std::vector<drm_property_enum_info> plane_type_enums {
    { .name = "Primary" },
    { .name = "Cursor"},
    { .name = "Overlay" }
  };

  static const std::vector<drm_property_info> connector_props = {
	{ .name = "EDID" },
	{
		.name = "DPMS",
    .num_enum_values = WDRM_DPMS_STATE_COUNT,
		.enum_values = dpms_state_enums
	},
	{ .name = "CRTC_ID", },
	{ .name = "non-desktop", },
};


  static const std::vector<drm_property_info> crtc_props {
	  { .name = "MODE_ID", },
	  { .name = "ACTIVE", },
	  { .name = "OUT_FENCE_PTR", },
  };

  static const std::vector<drm_property_info> plane_props {
    { 
      .name = "type",
      .num_enum_values = WDRM_PLANE_TYPE_COUNT,
      .enum_values = plane_type_enums
    },
    { .name = "SRC_X" },
    { .name = "SRC_Y"},
    { .name = "SRC_W" },
    { .name = "SRC_H" },
    { .name = "CRTC_X" },
    { .name = "CRTC_Y" },
    { .name = "CRTC_W" },
    { .name = "CRTC_H" },
    { .name = "FB_ID" },
    { .name = "CRTC_ID", },
	  { .name = "IN_FORMATS" },
	  { .name = "IN_FENCE_FD" },
  };

  /**
 * List of properties attached to DRM planes
 */
enum wdrm_plane_property {
	WDRM_PLANE_TYPE = 0,
	WDRM_PLANE_SRC_X,
	WDRM_PLANE_SRC_Y,
	WDRM_PLANE_SRC_W,
	WDRM_PLANE_SRC_H,
	WDRM_PLANE_CRTC_X,
	WDRM_PLANE_CRTC_Y,
	WDRM_PLANE_CRTC_W,
	WDRM_PLANE_CRTC_H,
	WDRM_PLANE_FB_ID,
	WDRM_PLANE_CRTC_ID,
	WDRM_PLANE_IN_FORMATS,
	WDRM_PLANE_IN_FENCE_FD,
	WDRM_PLANE_COUNT
};

/**
 * List of properties attached to DRM CRTCs
 */
enum wdrm_crtc_property {
	WDRM_CRTC_MODE_ID = 0,
	WDRM_CRTC_ACTIVE,
	WDRM_CRTC_OUT_FENCE_PTR,
	WDRM_CRTC_COUNT
};

/**
 * List of properties attached to a DRM connector
 */
enum wdrm_connector_property {
	WDRM_CONNECTOR_EDID = 0,
	WDRM_CONNECTOR_DPMS,
	WDRM_CONNECTOR_CRTC_ID,
	WDRM_CONNECTOR_NON_DESKTOP,
	WDRM_CONNECTOR_COUNT
};

struct props {
  std::vector<drm_property_info> plane;
  std::vector<drm_property_info> crtc;
  std::vector<drm_property_info> connector;
};

}