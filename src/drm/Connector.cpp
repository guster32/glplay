#include "Connector.hpp"

namespace glplay::drm {
  //Constructor
  Connector::Connector(int fileDesc, uint32_t connectorId): fileDesc(fileDesc), connectorId(connectorId), connector(drmModeGetConnector(fileDesc, connectorId)) {
    if (connector == nullptr) {
			throw std::runtime_error(std::string("Unable to get connector: ") + strerror(errno));
		}
  }

  //Destructor
  Connector::~Connector() {
    if (connector != nullptr) {
      drmModeFreeConnector(connector);
    }
  }

  //Copy constructor
  // Do deep copy
  Connector::Connector(const Connector& other): fileDesc(other.fileDesc), connectorId(other.connectorId), connector(other.connector) { 
    if (connector == nullptr) {
			throw std::runtime_error(std::string("Unable to get connector: ") + strerror(errno));
		}
  }

  // Move constructor
  // Transfer ownership 
  Connector::Connector(Connector&& other) noexcept : fileDesc(other.fileDesc), connectorId(other.connectorId), connector(other.connector) {
    other.fileDesc = -1;
    other.connectorId = -1;
    other.connector = nullptr;
  }

  // Copy assignment
  // Do deep copy 
  auto Connector::operator=(const Connector& other) -> Connector& {
    if (&other == this) {
      return *this;
    }
    if(connector != nullptr) {
      drmModeFreeConnector(connector);
    }
    fileDesc = other.fileDesc;
    connectorId = other.connectorId;
    connector = other.connector;
    return *this;
  }

  // Move assignment
  // Transfer ownership 
  auto Connector::operator=(Connector&& other) noexcept -> Connector& {
    if (&other == this) {
      return *this;
    }
    if(connector != nullptr) {
      drmModeFreeConnector(connector);
    }
    fileDesc = other.fileDesc;
    connectorId = other.connectorId;
    connector = other.connector;

    other.fileDesc = -1;
    other.connectorId = -1;
    other.connector = nullptr;
    return *this;
  }

  auto Connector::hasEncoder() -> bool {
    return connector->encoder_id != 0;
  }

}
