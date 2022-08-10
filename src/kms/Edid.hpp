#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>
#include <stdexcept>
#include <cstdio>
#include <algorithm>
#include <cstring>
#include <iterator>
#include <string>

#define EDID_DESCRIPTOR_ALPHANUMERIC_DATA_STRING 0xfe
#define EDID_DESCRIPTOR_DISPLAY_PRODUCT_NAME 0xfc
#define EDID_DESCRIPTOR_DISPLAY_PRODUCT_SERIAL_NUMBER 0xff
#define EDID_OFFSET_DATA_BLOCKS 0x36
#define EDID_OFFSET_LAST_BLOCK 0x6c
#define EDID_OFFSET_PNPID 0x08
#define EDID_OFFSET_SERIAL 0x0c

namespace glplay::kms {
  
  class Edid {
    public:
      std::array<char, 13> eisa_id;
	    std::array<char, 13> monitor_name;
	    std::array<char, 5> pnp_id;
	    std::array<char, 13> serial_number;

      explicit Edid(const uint8_t *data, size_t length);
      Edid(const Edid& other); //Copy constructor
      Edid(Edid&& other) noexcept; //Move constructor
      auto operator=(const Edid& other) -> Edid&; //Copy assignment
      auto operator=(Edid&& other) noexcept -> Edid&; //Move assignment
      ~Edid() = default;

    private:
      static auto parse_string(const uint8_t *data) -> std::array<char, 13>;
  };
}