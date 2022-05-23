#ifndef DISPLAYADAPTER_HEADER_H
#define DISPLAYADAPTER_HEADER_H

#include <xf86drmMode.h>
#include <gbm.h>

#include <fcntl.h>
#include <stdexcept>
#include <cstring>
#include <string>


class DisplayAdapter {
  public:
    explicit DisplayAdapter(const char* path);
    [[nodiscard]] auto fileDescriptor() const -> int { return fd; }
    [[nodiscard]] auto displayMode() const -> drmModeModeInfo* { return mode; }
    [[nodiscard]] auto crtc() const -> uint32_t { return crtcId; }
    [[nodiscard]] auto connector() const -> uint32_t { return connectorId; }
    [[nodiscard]] auto surface() const -> gbm_surface* { return bufferSurface; }
    [[nodiscard]] auto buffer() const -> gbm_device* { return bufferDevice; }
  private:
    int fd;
    drmModeModeInfo *mode = nullptr;
    drmModeRes *resources = nullptr;
    drmModeEncoder *encoder = nullptr;
    struct gbm_device *bufferDevice = nullptr;
	  struct gbm_surface *bufferSurface = nullptr;
    uint32_t connectorId = 0;
    uint32_t crtcId = 0;
    auto find_crtc_for_connector(const drmModeRes *resources, const drmModeConnector *connector) const -> uint32_t ;
    static auto find_crtc_for_encoder(const drmModeRes *resources, const drmModeEncoder *encoder) -> uint32_t;
};

#endif