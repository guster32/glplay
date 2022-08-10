#include "DisplayAdapter.hpp"

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
		
		for(int idx = 0; idx < resources->count_connectors; idx++) {
			auto connector = drm::Connector(fd, resources->connectors[idx]);
			
			if(connector->encoder_id == 0) {
				std::cout << "[CONN:" << connector->connector_id << "]: no encoder\n";
				continue;
			}
			auto encoder = findEncoderForConnector(resources, connector);
			auto crtc = findCrtcForEncoder(resources, encoder);
			auto plane = findPrimaryPlaneForCrtc(crtc);
			if(connector.hasEncoder() && encoder.hasCrtc() && crtc.isActive()) {
				displays.emplace_back(fd, connector, crtc, plane);
			}
		}
		if(displays.empty()) {
			throw std::runtime_error("Device has not active displays");
		}
  }

	auto DisplayAdapter::findEncoderForConnector(drm::Resources &resources, drm::Connector &connector) -> drm::Encoder {
		for(int idx = 0; idx < resources->count_encoders; idx++) {
      if (resources->encoders[idx] == connector->encoder_id) {
				return drm::Encoder(adapterFD.fileDescriptor(), resources->encoders[idx]);
			}
		}
		throw std::runtime_error("Unable to find encoder for connector");
	}

	auto DisplayAdapter::findCrtcForEncoder(drm::Resources &resources, drm::Encoder &encoder) -> drm::Crtc {
		for(int idx = 0; idx < resources->count_crtcs; idx++) {
      if (resources->crtcs[idx] == encoder->crtc_id) {
				return drm::Crtc(adapterFD.fileDescriptor(), resources->crtcs[idx]);
			}
		}
		throw std::runtime_error("Unable to find crtc for encoder");
	}

	auto DisplayAdapter::findPrimaryPlaneForCrtc(drm::Crtc &crtc) -> drm::Plane {
		for (auto &plane : planes){
			if(plane->crtc_id == crtc->crtc_id && plane->fb_id == crtc->buffer_id) {
				return plane;
			}
		}
		throw std::runtime_error("Unable to find plane for crtc");
	}


}
