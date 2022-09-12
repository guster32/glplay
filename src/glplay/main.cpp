#include "../kms/kms.hpp"
#include "../drm/drm.hpp"
#include "../gbm/gbm.hpp"
#include "../egl/egl.hpp"
#include <iostream>

auto main(int argc, char *argv[]) -> int {
	auto paths = glplay::drm::getDevicePaths();
	auto adapter = glplay::kms::DisplayAdapter(paths.at(0));
	auto buffer = glplay::gbm::make_gbm_ptr(adapter.getAdapterFD());
	//Create renderer here  vk_device_create or device_egl_setup or software
	auto *display = glplay::egl::device_egl_setup(buffer.get(), adapter.hasfbModifiersSupport());
  
	return 0;
}