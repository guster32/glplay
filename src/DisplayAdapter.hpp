#ifndef DISPLAYADAPTER_HEADER_HPP
#define DISPLAYADAPTER_HEADER_HPP

#include <xf86drmMode.h>
#include <xf86drm.h>
#include <gbm.h>

#include <fcntl.h>
#include <stdexcept>
#include <vector>
#include <cstring>
#include <string>

#include "../third-party/gsl/gsl"

class DisplayAdapter {
  public:
    explicit DisplayAdapter(char* path);
    [[nodiscard]] auto fileDescriptor() const -> int { return fd; }
    [[nodiscard]] auto displayMode() const -> drmModeModeInfo* { return mode; }
    [[nodiscard]] auto crtc() const -> uint32_t { return crtcId; }
    [[nodiscard]] auto connector() const -> uint32_t { return connectorId; }
    [[nodiscard]] auto surface() const -> gbm_surface* { return bufferSurface; }
    [[nodiscard]] auto buffer() const -> gbm_device* { return bufferDevice; }
    [[nodiscard]] static auto getDevices() -> std::vector<std::string>;
  private:
    int fd;
    drmModeModeInfo *mode = nullptr;
    drmModeRes *resources = nullptr;
    drmModeEncoder *encoder = nullptr;
    struct gbm_device *bufferDevice = nullptr;
	  struct gbm_surface *bufferSurface = nullptr;
    uint32_t connectorId = 0;
    uint32_t crtcId = 0;
    auto findCrtcForConnector(const drmModeRes *resources, const drmModeConnector *connector) const -> uint32_t ;
    static auto findCrtcForEncoder(const drmModeRes *resources, const drmModeEncoder *encoder) -> uint32_t;
};

#endif