#include "../kms/kms.hpp"
#include "../drm/drm.hpp"
#include "../nix/nix.hpp"
#include <iostream>

auto main(int argc, char *argv[]) -> int {
	auto paths = glplay::drm::getDevicePaths();
	auto adapter = glplay::kms::DisplayAdapter(paths.at(0));
	//auto vt_fd = glplay::nix::vt_setup();
	//glplay::nix::vt_reset(vt_fd);
  
	return 0;
}