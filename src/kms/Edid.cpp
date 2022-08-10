#include "Edid.hpp"

namespace glplay::kms {

  //Constructor
  Edid::Edid(const uint8_t *data, size_t length) {
    int idx = 0;
    uint32_t serial_number = 0;
    
    /* check header */
    if (length < 128) {
      throw std::runtime_error(std::string("Unable to parse Edid "));
    }
    if (data[0] != 0x00 || data[1] != 0xff) {
      throw std::runtime_error(std::string("Unable to parse Edid "));
    }

    /* decode the PNP ID from three 5 bit words packed into 2 bytes
    * /--08--\/--09--\
    * 7654321076543210
    * |\---/\---/\---/
    * R  C1   C2   C3 */
    this->pnp_id[0] = 'A' + ((data[EDID_OFFSET_PNPID + 0] & 0x7c) / 4) - 1;
    this->pnp_id[1] = 'A' + ((data[EDID_OFFSET_PNPID + 0] & 0x3) * 8) + ((data[EDID_OFFSET_PNPID + 1] & 0xe0) / 32) - 1;
    this->pnp_id[2] = 'A' + (data[EDID_OFFSET_PNPID + 1] & 0x1f) - 1;
    this->pnp_id[3] = '\0';

    /* maybe there isn't a ASCII serial number descriptor, so use this instead */
    serial_number = static_cast<uint32_t>(data[EDID_OFFSET_SERIAL + 0]);
    serial_number += static_cast<uint32_t>(data[EDID_OFFSET_SERIAL + 1]) * 0x100;
    serial_number += static_cast<uint32_t>(data[EDID_OFFSET_SERIAL + 2]) * 0x10000;
    serial_number += static_cast<uint32_t>(data[EDID_OFFSET_SERIAL + 3]) * 0x1000000;
    if (serial_number > 0) {
      sprintf(this->serial_number.data(), "%lu", static_cast<unsigned long>(serial_number));
    }

    /* parse EDID data */
    for (idx = EDID_OFFSET_DATA_BLOCKS; idx <= EDID_OFFSET_LAST_BLOCK;
        idx += 18) {
      /* ignore pixel clock data */
      if (data[idx] != 0) {
        continue;
      }
      if (data[idx+2] != 0) {
        continue;
      }

      /* any useful blocks? */
      if (data[idx+3] == EDID_DESCRIPTOR_DISPLAY_PRODUCT_NAME) {
        auto monitor_name = parse_string(&data[idx+5]);
        std::copy(std::begin(monitor_name), std::end(monitor_name), std::begin(this->monitor_name));
      } else if (data[idx+3] == EDID_DESCRIPTOR_DISPLAY_PRODUCT_SERIAL_NUMBER) {
        auto serial_number = parse_string(&data[idx+5]);
        std::copy(std::begin(serial_number), std::end(serial_number), std::begin(this->serial_number));
      } else if (data[idx+3] == EDID_DESCRIPTOR_ALPHANUMERIC_DATA_STRING) {
        auto eisa_id = parse_string(&data[idx+5]);
        std::copy(std::begin(eisa_id), std::end(eisa_id), std::begin(this->eisa_id));
      }
    }
  }

  //Copy constructor
  // Do deep copy
  Edid::Edid(const Edid& other) = default;


  // Move constructor
  // Transfer ownership 
  Edid::Edid(Edid&& other) noexcept : eisa_id(other.eisa_id), monitor_name(other.monitor_name), pnp_id(other.pnp_id), serial_number(other.serial_number) {
    std::fill( std::begin(other.eisa_id), std::end(other.eisa_id), 0 );
    std::fill( std::begin(other.monitor_name), std::end(other.monitor_name), 0 );
    std::fill( std::begin(other.pnp_id), std::end(other.pnp_id), 0 );
    std::fill( std::begin(other.serial_number), std::end(other.serial_number), 0 );
  }

  // Copy assignment
  // Do deep copy 
  auto Edid::operator=(const Edid& other) -> Edid& {
    if (&other == this) {
      return *this;
    }

    std::copy(std::begin(other.eisa_id), std::end(other.eisa_id), std::begin(eisa_id));
    std::copy(std::begin(other.monitor_name), std::end(other.monitor_name), std::begin(monitor_name));
    std::copy(std::begin(other.pnp_id), std::end(other.pnp_id), std::begin(pnp_id));
    std::copy(std::begin(other.serial_number), std::end(other.serial_number), std::begin(serial_number));
    return *this;
  }

  // Move assignment
  // Transfer ownership 
  auto Edid::operator=(Edid&& other) noexcept -> Edid& {
    if (&other == this) {
      return *this;
    }
    
    std::copy(std::begin(other.eisa_id), std::end(other.eisa_id), std::begin(eisa_id));
    std::copy(std::begin(other.monitor_name), std::end(other.monitor_name), std::begin(monitor_name));
    std::copy(std::begin(other.pnp_id), std::end(other.pnp_id), std::begin(pnp_id));
    std::copy(std::begin(other.serial_number), std::end(other.serial_number), std::begin(serial_number));

    std::fill( std::begin(other.eisa_id), std::end(other.eisa_id), 0 );
    std::fill( std::begin(other.monitor_name), std::end(other.monitor_name), 0 );
    std::fill( std::begin(other.pnp_id), std::end(other.pnp_id), 0 );
    std::fill( std::begin(other.serial_number), std::end(other.serial_number), 0 );
    return *this;
  }

  inline auto Edid::parse_string(const uint8_t *data) -> std::array<char, 13> {
   	int idx = 0;
    int replaced = 0;
    auto text = std::array<char, 13>{};

    /* this is always 12 bytes, but we can't guarantee it's null
    * terminated or not junk. */
    strncpy(text.data(), reinterpret_cast<const char *>(data), 12);

    /* guarantee our new string is null-terminated */
    text[12] = '\0';

    /* remove insane chars */
    for (idx = 0; text[idx] != '\0'; idx++) {
      if (text[idx] == '\n' || text[idx] == '\r') {
        text[idx] = '\0';
        break;
      }
    }

    /* ensure string is printable */
    for (idx = 0; text[idx] != '\0'; idx++) {
      if (!isprint(text[idx])) {
        text[idx] = '-';
        replaced++;
      }
    }

    /* if the string is random junk, ignore the string */
    if (replaced > 4) {
      text[0] = '\0';
    }
    return text;
  }
}