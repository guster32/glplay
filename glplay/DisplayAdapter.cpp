#include "DisplayAdapter.h"
#include <xf86drm.h>

DisplayAdapter::DisplayAdapter(const char* path) : fd(open(path, O_RDWR)) {

	int err = drmSetClientCap(fd, DRM_CLIENT_CAP_UNIVERSAL_PLANES, 1);
	err |= drmSetClientCap(fd, DRM_CLIENT_CAP_ATOMIC, 1);
	
	if (err != 0) {
		throw std::runtime_error(std::string("drmModeGetResources failed: ") + strerror(errno));
	}

	drmModeConnector *connector = nullptr;
	
	if (fd < 0) {
		throw std::runtime_error("could not open drm device");
	}
	resources = drmModeGetResources(fd);
	
	if (resources == nullptr) {
		throw std::runtime_error(std::string("drmModeGetResources failed: ") + strerror(errno));
	}

	/* find a connected connector: */
	for (int idx = 0; idx < resources->count_connectors; idx++) {
		connector = drmModeGetConnector(fd, resources->connectors[idx]);
		if (connector->connection == DRM_MODE_CONNECTED) {
			/* it's connected, let's use this! */
			break;
		}
		drmModeFreeConnector(connector);
		connector = nullptr;
	}

		if (connector == nullptr) {
		/* we could be fancy and listen for hotplug events and wait for
		 * a connector..
		 */
		throw std::runtime_error("no connected connector!");
	}

		/* find prefered mode or the highest resolution mode: */
	for (int idx = 0, area = 0; idx < connector->count_modes; idx++) {
		drmModeModeInfo *current_mode = &connector->modes[idx];

		if ((current_mode->type & DRM_MODE_TYPE_PREFERRED) != 0U) {
			mode = current_mode;
		}

		int current_area = current_mode->hdisplay * current_mode->vdisplay;
		if (current_area > area) {
			mode = current_mode;
			area = current_area;
		}
	}
	
	if (mode == nullptr) {
		throw std::runtime_error("could not find mode!");
	}

		/* find encoder: */
	for (int idx = 0; idx < resources->count_encoders; idx++) {
		encoder = drmModeGetEncoder(fd, resources->encoders[idx]);
		if (encoder->encoder_id == connector->encoder_id) {
			break;
		}
		drmModeFreeEncoder(encoder);
		encoder = nullptr;
	}

	if (encoder != nullptr) {
		crtcId = encoder->crtc_id;
	} else {
		uint32_t crtc_id = find_crtc_for_connector(resources, connector);
		if (crtc_id == 0) {
			throw std::runtime_error("no crtc found!");
		}
		crtc_id = crtc_id;
	}
	connectorId = connector->connector_id;

	bufferDevice = gbm_create_device(fd);
	bufferSurface = gbm_surface_create(bufferDevice,
		mode->hdisplay, mode->vdisplay,
		GBM_FORMAT_XRGB8888,
		GBM_BO_USE_SCANOUT | GBM_BO_USE_RENDERING);
	
	if (bufferSurface == nullptr) {
    throw std::runtime_error("failed to create gbm surface");
	}

}

auto DisplayAdapter::find_crtc_for_encoder(const drmModeRes *resources, const drmModeEncoder *encoder) -> uint32_t {
	uint32_t idx = 0;

	for (idx = 0; idx < resources->count_crtcs; idx++) {
		/* possible_crtcs is a bitmask as described here:
		 * https://dvdhrm.wordpress.com/2012/09/13/linux-drm-mode-setting-api
		 */
		const uint32_t crtc_mask = 1U << idx;
		const uint32_t crtc_id = resources->crtcs[idx];
		if ((encoder->possible_crtcs & crtc_mask) != 0U) {
			return crtc_id;
		}
	}

	/* no match found */
	return -1;
}

auto DisplayAdapter::find_crtc_for_connector(const drmModeRes *resources, const drmModeConnector *connector) const -> uint32_t {
	int idx = 0;

	for (idx = 0; idx < connector->count_encoders; idx++) {
		const uint32_t encoder_id = connector->encoders[idx];
		drmModeEncoder *encoder = drmModeGetEncoder(fd, encoder_id);

		if (encoder != nullptr) {
			const uint32_t crtc_id = find_crtc_for_encoder(resources, encoder);

			drmModeFreeEncoder(encoder);
			if (crtc_id != 0) {
				return crtc_id;
			}
		}
	}

	/* no match found */
	return -1;
}