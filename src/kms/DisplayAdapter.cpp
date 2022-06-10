#include "DisplayAdapter.hpp"
#include "drm.h"


namespace glplay::kms {
  DisplayAdapter::DisplayAdapter(std::string &path): adapterFD(path, O_RDWR | O_CLOEXEC) {
    
    uint64_t cap = 0;
    drm_magic_t magic = 0;
    auto fd = adapterFD.fileDescriptor();
    auto filename = adapterFD.filePath();

		if(drmGetMagic(fd, &magic) !=0 || drmAuthMagic(fd, magic) != 0){
			throw std::runtime_error(std::string("Device ")+ 
				adapterFD.filePath() + " is not master");
		}

    uint err = 
			drmSetClientCap(fd, DRM_CLIENT_CAP_UNIVERSAL_PLANES, 1)
		| drmSetClientCap(fd, DRM_CLIENT_CAP_ATOMIC, 1);
		if (err != 0) {
			throw std::runtime_error("Device " + filename + "  either does not support for universal planes or atomic commits");
		}
    
    err = drmGetCap(fd, DRM_CAP_ADDFB2_MODIFIERS, &cap);
		supportsFBModifiers = (err == 0 && cap !=0);

    auto resources = drm::Resources(fd);
    auto planeResources = drm::PlaneResources(fd);

    if (resources->count_crtcs <= 0 || resources->count_connectors <= 0 ||
				resources->count_encoders <= 0 || planeResources->count_planes <= 0) {
      throw std::runtime_error("device " + filename + "is not a KMS device");
		}

    for(uint32_t idx = 0; idx < planeResources->count_planes; ++idx) {
			planes.emplace_back(fd, planeResources->planes[idx]);
    }
		printf("Im done here!! lets cleanup");


  }

  // void DisplayAdapter::openDevice() {












	// 	drmModeConnector *connector = nullptr;

	// 	/* find a connected connector: */
	// 	for (int idx = 0; idx < resources->count_connectors; idx++) {
	// 		connector = drmModeGetConnector(fd, resources->connectors[idx]);
	// 		if (connector->connection == DRM_MODE_CONNECTED) {
	// 			/* it's connected, let's use this! */
	// 			break;
	// 		}
	// 		drmModeFreeConnector(connector);
	// 		connector = nullptr;
	// 	}

	// 		if (connector == nullptr) {
	// 		/* we could be fancy and listen for hotplug events and wait for
	// 		* a connector..
	// 		*/
	// 		throw std::runtime_error("no connected connector!");
	// 	}

	// 		/* find prefered mode or the highest resolution mode: */
	// 	for (int idx = 0, area = 0; idx < connector->count_modes; idx++) {
	// 		drmModeModeInfo *current_mode = &connector->modes[idx];

	// 		if ((current_mode->type & DRM_MODE_TYPE_PREFERRED) != 0U) {
	// 			mode = current_mode;
	// 		}

	// 		int current_area = current_mode->hdisplay * current_mode->vdisplay;
	// 		if (current_area > area) {
	// 			mode = current_mode;
	// 			area = current_area;
	// 		}
	// 	}
		
	// 	if (mode == nullptr) {
	// 		throw std::runtime_error("could not find mode!");
	// 	}

	// 		/* find encoder: */
	// 	for (int idx = 0; idx < resources->count_encoders; idx++) {
	// 		encoder = drmModeGetEncoder(fd, resources->encoders[idx]);
	// 		if (encoder->encoder_id == connector->encoder_id) {
	// 			break;
	// 		}
	// 		drmModeFreeEncoder(encoder);
	// 		encoder = nullptr;
	// 	}

	// 	if (encoder != nullptr) {
	// 		crtcId = encoder->crtc_id;
	// 	} else {
	// 		uint32_t crtc_id = findCrtcForConnector(resources, connector);
	// 		if (crtc_id == 0) {
	// 			throw std::runtime_error("no crtc found!");
	// 		}
	// 		crtc_id = crtc_id;
	// 	}
	// 	connectorId = connector->connector_id;

	// 	bufferDevice = gbm_create_device(fd);
	// 	bufferSurface = gbm_surface_create(bufferDevice,
	// 		mode->hdisplay, mode->vdisplay,
	// 		GBM_FORMAT_XRGB8888,
	// 		GBM_BO_USE_SCANOUT | GBM_BO_USE_RENDERING);
		
	// 	if (bufferSurface == nullptr) {
	// 		throw std::runtime_error("failed to create gbm surface");
	// 	}
	// }

}
