// SPDX-License-Identifier: GPL-3.0-only
// ROM-search algorithm adapted from ESPHome's gpio/one_wire (GPLv3).
// Copyright (c) 2019 ESPHome; nRF transport modifications (c) 2026 maniekes.
// Published as a separate component on 2026-11-10; see NOTICE.md.
// Timing: Analog Devices, "1-Wire Communication Through Software", standard speed.
// nRF52 ESPHome 2026.9.x delayMicroseconds() uses k_usleep(): unsuitable here.
// Zephyr's native w1 GPIO driver also uses k_busy_wait in critical sections.
#include "nrf52_one_wire.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include <hal/nrf_gpio.h>
#include <zephyr/kernel.h>

namespace esphome::nrf52_one_wire {

static const char *const TAG = "nrf52.one_wire";

void NRF52OneWireBus::setup() {
  if (this->pin_ == nullptr || this->pin_->is_inverted() || this->pin_->get_pin() >= 48) {
    ESP_LOGE(TAG, "Requires a non-inverted nRF52840 GPIO");
    this->mark_failed();
    return;
  }
  this->pin_number_ = this->pin_->get_pin();
  // Configure once. S0D1 drives LOW and releases HIGH. Keep the input buffer
  // connected while output is enabled, so presence/data can always be sampled.
  // No opaque ISRPinArg layout, GPIO driver dispatch, or mode changes per bit.
  nrf_gpio_pin_set(this->pin_number_);
  nrf_gpio_cfg(this->pin_number_, NRF_GPIO_PIN_DIR_OUTPUT, NRF_GPIO_PIN_INPUT_CONNECT,
               this->internal_pullup_ ? NRF_GPIO_PIN_PULLUP : NRF_GPIO_PIN_NOPULL,
               NRF_GPIO_PIN_S0D1, NRF_GPIO_PIN_NOSENSE);
  this->ready_ = true;
  k_busy_wait(1000);
  this->search();
}

int NRF52OneWireBus::reset_int() {
  if (!this->ready_)
    return this->last_reset_ = -1;
  nrf_gpio_pin_set(this->pin_number_);
  // Allow up to 250 us for an idle bus to rise. No scheduler/tick-based timers.
  uint16_t retries = 250;
  while (!nrf_gpio_pin_read(this->pin_number_)) {
    if (retries-- == 0)
      return this->last_reset_ = -1;
    k_busy_wait(1);
  }
  bool presence;
  {
    InterruptLock lock;
    nrf_gpio_pin_clear(this->pin_number_);
    k_busy_wait(480);
    nrf_gpio_pin_set(this->pin_number_);
    // DS18B20 starts presence 15-60 us after release, holds it for 60-240 us.
    k_busy_wait(70);
    presence = !nrf_gpio_pin_read(this->pin_number_);
  }
  // Recovery is a minimum: interrupts are allowed once presence was sampled.
  k_busy_wait(410);
  if (!nrf_gpio_pin_read(this->pin_number_))
    return this->last_reset_ = -1;  // stuck LOW is not a valid presence pulse
  return this->last_reset_ = presence ? 1 : 0;
}

void NRF52OneWireBus::write_bit_(bool value) {
  {
    InterruptLock lock;
    nrf_gpio_pin_clear(this->pin_number_);
    k_busy_wait(value ? 6 : 60);
    nrf_gpio_pin_set(this->pin_number_);
  }
  k_busy_wait(value ? 64 : 10);
}

bool NRF52OneWireBus::read_bit_() {
  bool value;
  {
    InterruptLock lock;
    nrf_gpio_pin_clear(this->pin_number_);
    k_busy_wait(3);
    nrf_gpio_pin_set(this->pin_number_);
    k_busy_wait(10);
    value = nrf_gpio_pin_read(this->pin_number_) != 0;
  }
  // Sample near 13 us from the falling edge (DS18B20 requires <15 us), then
  // complete a 70 us slot. IRQs are blocked for one bit, never an entire ROM.
  k_busy_wait(57);
  return value;
}

void NRF52OneWireBus::write8(uint8_t value) {
  for (uint8_t bit = 0; bit < 8; bit++)
    this->write_bit_((value >> bit) & 1U);
}

void NRF52OneWireBus::write64(uint64_t value) {
  for (uint8_t bit = 0; bit < 64; bit++)
    this->write_bit_((value >> bit) & 1ULL);
}

uint8_t NRF52OneWireBus::read8() {
  uint8_t value = 0;
  for (uint8_t bit = 0; bit < 8; bit++)
    value |= uint8_t(this->read_bit_()) << bit;
  return value;
}

uint64_t NRF52OneWireBus::read64() {
  uint64_t value = 0;
  for (uint8_t bit = 0; bit < 64; bit++)
    value |= uint64_t(this->read_bit_()) << bit;
  return value;
}

void NRF52OneWireBus::reset_search() {
  this->last_discrepancy_ = 0;
  this->last_device_flag_ = false;
  this->address_ = 0;
  this->search_failed_bit_ = 0;
}

uint64_t NRF52OneWireBus::search_int() {
  if (this->last_device_flag_)
    return 0;
  uint8_t last_zero = 0;
  uint64_t address = this->address_;
  for (uint8_t bit = 1; bit <= 64; bit++) {
    const uint64_t mask = 1ULL << (bit - 1);
    const bool id_bit = this->read_bit_();
    const bool complement = this->read_bit_();
    if (id_bit && complement) {
      this->search_failed_bit_ = bit;
      return 0;
    }
    bool branch;
    if (id_bit != complement) {
      branch = id_bit;
    } else {
      branch = bit < this->last_discrepancy_ ? (address & mask) != 0 : bit == this->last_discrepancy_;
      if (!branch)
        last_zero = bit;
    }
    if (branch)
      address |= mask;
    else
      address &= ~mask;
    this->write_bit_(branch);
  }
  this->last_discrepancy_ = last_zero;
  this->last_device_flag_ = last_zero == 0;
  this->address_ = address;
  return address;
}

void NRF52OneWireBus::dump_config() {
  ESP_LOGCONFIG(TAG, "Native nRF52 1-Wire v2: busy-wait timing, direct GPIO");
  LOG_PIN("  Pin: ", this->pin_);
  ESP_LOGCONFIG(TAG, "  Internal pull-up: %s", this->internal_pullup_ ? "ON" : "OFF");
  ESP_LOGCONFIG(TAG, "  Last reset: %d; failed search bit: %u", this->last_reset_, this->search_failed_bit_);
  this->dump_devices_(TAG);
}

void NRF52OneWireBus::diagnose() {
  if (!this->ready_ || !this->devices_.empty())
    return;
  nrf_gpio_pin_set(this->pin_number_);
  k_busy_wait(250);
  const bool idle = nrf_gpio_pin_read(this->pin_number_) != 0;
  const int presence = this->reset_int();
  ESP_LOGI(TAG, "P%u.%02u idle=%d reset=%d (-1=stuck LOW, 0=no presence, 1=presence); search_bit=%u",
           this->pin_number_ / 32, this->pin_number_ % 32, idle, presence, this->search_failed_bit_);
}

}  // namespace esphome::nrf52_one_wire
