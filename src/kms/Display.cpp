#include "Display.hpp"
#include "Edid.hpp"
#include <cstdint>

namespace glplay::kms {

  Display::Display(int adapterFD, drm::Connector &connector, drm::Crtc &crtc, drm::Plane &plane): connector(connector), mode(crtc->mode), primary_plane(plane), crtc(crtc), explicitFencing(false) {
    
    auto refresh = ((crtc->mode.clock * 1000000LL / crtc->mode.htotal) +
		  (crtc->mode.vtotal / 2)) / crtc->mode.vtotal;
    
    name = ((connector->connector_type < drm::connectorTypes.size()) ?
		 	drm::connectorTypes.at(connector->connector_type) :
			"UNKNOWN") + "-" + std::to_string(connector->connector_type_id);

    refreshIntervalNsec = millihzToNsec(refresh);
    
    mode_blob_id = drm::mode_blob_create(adapterFD, &mode);
    
    auto *planeProps = drmModeObjectGetProperties(adapterFD, plane->plane_id, DRM_MODE_OBJECT_PLANE);
    drm::drm_property_info_populate(adapterFD, drm::plane_props, this->props.plane, drm::plane_props.size(), planeProps);
    this->plane_formats_populate(adapterFD, planeProps);

    auto *crtProps = drmModeObjectGetProperties(adapterFD, crtc->crtc_id, DRM_MODE_OBJECT_CRTC);
    drm::drm_property_info_populate(adapterFD, drm::crtc_props, this->props.crtc, drm::crtc_props.size(), crtProps);

    auto *connectorProps = drmModeObjectGetProperties(adapterFD, connector->connector_id, DRM_MODE_OBJECT_CONNECTOR);
    drm::drm_property_info_populate(adapterFD, drm::connector_props, this->props.connector, drm::connector_props.size(), connectorProps);

    this->get_edid(adapterFD, connectorProps);

    /*
	  * Set if we support explicit fencing inside KMS; the EGL renderer will
	  * clear this if it doesn't support it.
	  */
	  this->explicitFencing =
		((this->props.plane[drm::WDRM_PLANE_IN_FENCE_FD].prop_id != 0U) &&
		 (this->props.crtc[drm::WDRM_CRTC_OUT_FENCE_PTR].prop_id != 0U));

  }

  void Display::get_edid(int adapterFD, drmModeObjectPropertiesPtr props) {
    drmModePropertyBlobPtr blob = nullptr;
	  	  uint32_t blob_id = 0;
	  int ret = 0;

	  blob_id = drm_property_get_value(&this->props.connector[drm::WDRM_CONNECTOR_EDID], props, 0);
    if (blob_id == 0) {
      debug("[%s] output does not have EDID\n", output->name);
      return;
    }

	  blob = drmModeGetPropertyBlob(adapterFD, blob_id);
	
    auto edid = Edid(static_cast<const uint8_t*>(blob->data), blob->length);
    drmModeFreePropertyBlob(blob);

    debug("[%s] EDID PNP ID %s, EISA ID %s, name %s, serial %s\n",
      output->name, edid->pnp_id, edid->eisa_id,
      edid->monitor_name, edid->serial_number);
    
  }

  void Display::plane_formats_populate(int adapterFD, drmModeObjectPropertiesPtr props) {


    uint32_t blob_id = drm::drm_property_get_value(&this->props.plane[drm::WDRM_PLANE_IN_FORMATS], props, 0);

    if(blob_id == 0) {
      debug("[%s] plane does not have IN_FORMATS\n", this->name);
		  return;
    }
    
    auto *blob = drmModeGetPropertyBlob(adapterFD, blob_id);

    auto *fmt_mod_blob = static_cast<drm_format_modifier_blob*>(blob->data);
    auto *blob_formats = formats_ptr(fmt_mod_blob);
    auto *blob_modifiers = modifiers_ptr(fmt_mod_blob);

    for (unsigned int idx = 0; idx < fmt_mod_blob->count_formats; idx++) {
      if (blob_formats[idx] != DRM_FORMAT_XRGB8888) {
        continue;
      }

      for (unsigned int idx1 = 0; idx1 < fmt_mod_blob->count_modifiers; idx1++) {
        struct drm_format_modifier *mod = &blob_modifiers[idx1];

        if ((idx < mod->offset) || (idx > mod->offset + 63)) {
          continue;
        }
        if ((mod->formats & (1 << (idx - mod->offset))) == 0U) {
          continue;
        }
        // I think this is a simple vector of .
        this->modifiers.emplace_back(mod->modifier);
      }
    }
	  drmModeFreePropertyBlob(blob);
  }
}