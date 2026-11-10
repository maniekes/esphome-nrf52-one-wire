#pragma once
#include <cstdint>
#include <cstdio>
#include <cstddef>
void test_lock();
void test_unlock();
namespace esphome {
class InterruptLock {
 public:
  InterruptLock() { test_lock(); }
  ~InterruptLock() { test_unlock(); }
};
inline uint8_t crc8(const uint8_t *data, size_t len) {
  uint8_t crc = 0;
  while (len--) {
    uint8_t value = *data++;
    for (int i = 0; i < 8; ++i) {
      const bool odd = (crc ^ value) & 1;
      crc >>= 1;
      if (odd) crc ^= 0x8c;
      value >>= 1;
    }
  }
  return crc;
}
inline const char *format_hex_to(char *out, uint64_t value) {
  std::snprintf(out, 17, "%016llx", static_cast<unsigned long long>(value));
  return out;
}
}  // namespace esphome
