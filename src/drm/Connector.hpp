#include <string>
#include <cstring>
#include <stdexcept>

#include <xf86drmMode.h>

namespace glplay::drm {

  class Connector {
    public:
      explicit Connector(int fileDesc, int connectorId);
      Connector(const Connector& other); //Copy constructor
      Connector(Connector&& other) noexcept; //Move constructor
      auto operator=(const Connector& other) -> Connector&; //Copy assignment
      auto operator=(Connector&& other) noexcept -> Connector&; //Move assignment
      [[nodiscard]] auto operator->() const -> drmModeConnectorPtr { return connector; }

      ~Connector();
    private:
      int fileDesc = -1;
      int connectorId = -1;
      drmModeConnectorPtr connector = nullptr;
  };

}