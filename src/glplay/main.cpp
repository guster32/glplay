#include "../kms/kms.hpp"
#include "../drm/drm.hpp"
#include "../gbm/gbm.hpp"
#include "../nix/nix.hpp"
#include <iostream>

auto main(int argc, char *argv[]) -> int {
	auto paths = glplay::drm::getDevicePaths();
	auto adapter = glplay::kms::DisplayAdapter(paths.at(0));
	//Create renderer here  vk_device_create or device_egl_setup or software
	
	//auto egl= glplay::egl::output_egl_setup(display);
	auto vt_fd = glplay::nix::vt_setup();
	glplay::nix::vt_reset(vt_fd);
  
	return 0;
}