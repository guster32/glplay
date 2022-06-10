#include "../kms/kms.hpp"
#include "../drm/drm.hpp"

auto main(int argc, char *argv[]) -> int {
	auto paths = glplay::drm::getDevicePaths();
	auto adapter = glplay::kms::DisplayAdapter(paths.at(0));
	return 0;
}