#pragma once

#include "DisplayAdapter.hpp"


/* Create a dmabuf FD from a GEM handle. */
inline auto handle_to_fd(int kms_fd, uint32_t gem_handle) -> int {
	struct drm_prime_handle prime = {
		.handle = gem_handle,
		.flags = DRM_RDWR | DRM_CLOEXEC,
	};
	int ret = 0;

	ret = ioctl(kms_fd, DRM_IOCTL_PRIME_HANDLE_TO_FD, &prime);
	if (ret != 0) {
		error("failed to export GEM handle %" PRIu32 " to FD\n", gem_handle);
		return -1;
	}

	return prime.fd;
}
