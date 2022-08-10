#pragma once

#include <cstdint>
#include <string>
#include <cstring>
#include <stdexcept>

#include <xf86drmMode.h>

namespace glplay::drm {

  class Connector {
    public:
      explicit Connector(int fileDesc, uint32_t connectorId);
      Connector(const Connector& other); //Copy constructor
      Connector(Connector&& other) noexcept; //Move constructor
      auto operator=(const Connector& other) -> Connector&; //Copy assignment
      auto operator=(Connector&& other) noexcept -> Connector&; //Move assignment
      [[nodiscard]] auto operator->() const -> drmModeConnectorPtr { return connector; }
      auto hasEncoder() -> bool;

      ~Connector();
    private:
      int fileDesc = -1;
      uint32_t connectorId = -1;
      drmModeConnectorPtr connector = nullptr;
  };

}