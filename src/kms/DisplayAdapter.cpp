#include "DisplayAdapter.hpp"
#include "Display.hpp"

namespace glplay::kms {
  DisplayAdapter::DisplayAdapter(std::string &path): adapterFD(path, O_RDWR | O_CLOEXEC),
		//InitDrmDevice
		gbmDevice(gbm::make_gbm_ptr(adapterFD.fileDescriptor())),
		eglDevice(gbmDevice) {
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
		bool supportsFBModifiers = (err == 0 && cap !=0);

    auto resources = drm::make_resources_ptr(fd);
		
		for(int idx = 0; idx < resources->count_connectors; idx++) {
			auto connectorId = resources->connectors[idx];
			auto connector = drm::make_connetor_ptr(fd, connectorId);
			
			if(connector->encoder_id == 0) {
				std::cout << "[CONN:" << connector->connector_id << "]: no encoder\n";
				continue;
			}
			if(connector->encoder_id != 0 /* && encoder->encoder_id != 0 && crtc->buffer_id != 0*/) {
				displays.emplace_back(fd, connectorId, resources);
				displays.back().createEGLBuffers(fd, supportsFBModifiers, eglDevice, gbmDevice);
			}
		}
		if(displays.empty()) {
			throw std::runtime_error("Device has not active displays");
		}
  }
}
