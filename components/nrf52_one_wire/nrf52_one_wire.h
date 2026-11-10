#pragma once

#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/components/one_wire/one_wire.h"

namespace esphome::nrf52_one_wire {

class NRF52OneWireBus final : public one_wire::OneWireBus, public Component {
 public:
  void set_pin(InternalGPIOPin *pin) { this->pin_ = pin; }
  void set_internal_pullup(bool enabled) { this->internal_pullup_ = enabled; }
  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::BUS; }
  void write8(uint8_t value) override;
  void write64(uint64_t value) override;
  uint8_t read8() override;
  uint64_t read64() override;
  // Run outside conversions, only when no device was found at startup.
  void diagnose();

 protected:
  int reset_int() override;
  void reset_search() override;
  uint64_t search_int() override;
  void write_bit_(bool value);
  bool read_bit_();

  InternalGPIOPin *pin_{nullptr};
  uint32_t pin_number_{0};
  bool internal_pullup_{true};
  bool ready_{false};
  uint64_t address_{0};
  uint8_t last_discrepancy_{0};
  bool last_device_flag_{false};
  int last_reset_{0};
  uint8_t search_failed_bit_{0};
};

}  // namespace esphome::nrf52_one_wire
