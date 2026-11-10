#pragma once
#include <cstdint>
#define IRAM_ATTR
namespace esphome {
class InternalGPIOPin {
 public:
  uint8_t number{29};
  bool inverted{false};
  uint8_t get_pin() const { return number; }
  bool is_inverted() const { return inverted; }
};
// The legacy sleeping delay must never be used in this transport.
void delayMicroseconds(uint32_t) = delete;
}
