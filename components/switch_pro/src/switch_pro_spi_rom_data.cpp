#include "switch_pro_spi_rom_data.hpp"

#include <cstring>

using namespace sp;

int sp::read_spi(uint8_t bank, uint8_t reg, uint8_t read_length, uint8_t *response) {
  if (bank == REG_BANK_SHIPMENT) {
    // set the byte(s) to 0
    for (int i = 0; i < read_length; i++) {
      response[i] = 0;
    }
  } else if (bank == REG_BANK_FACTORY_CONFIG) {
    // copy from sp::spi_rom_data_60 variable
    std::memcpy(response, spi_rom_data_60 + reg, read_length);
    return read_length;
  } else if (bank == REG_BANK_USER_CAL) {
    // copy from sp::spi_rom_data_80 variable
    std::memcpy(response, spi_rom_data_80 + reg, read_length);
    return read_length;
  }
  return 0;
}
