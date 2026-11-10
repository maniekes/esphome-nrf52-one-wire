// Native tests execute the actual production transport + ESPHome ROM search.
// Fake devices respond to edges/commands on a microsecond simulation clock.
#include "nrf52_one_wire.h"
#include "esphome/core/helpers.h"
#include <hal/nrf_gpio.h>
#include <algorithm>
#include <array>
#include <cassert>
#include <cstdio>
#include <deque>
#include <vector>

struct Wire {
  uint64_t now{0}, low_start{0}, reset_release{0}, lock_start{0};
  uint64_t max_lock{0};
  bool low{false}, stuck{false}, stuck_after_reset{false}, configured{false};
  bool have_reset{false}, read_slot{false}, search_mode{false};
  bool slot_value{true};
  int lock_depth{0}, pull{0}, rom_bit{0}, search_reads{0};
  uint8_t command{0}, command_bits{0};
  std::vector<uint64_t> devices, participants;
  std::vector<unsigned> pulses;
  std::vector<unsigned> samples;
  std::deque<bool> raw_reads;
  uint32_t configured_pin{0};

  void write(bool value) {
    if (!search_mode) {
      command |= unsigned(value) << command_bits++;
      if (command_bits == 8) {
        search_mode = command == 0xf0;
        command = command_bits = 0;
      }
    } else {
      assert(search_reads == 2);
      std::erase_if(participants, [&](uint64_t rom) { return bool((rom >> rom_bit) & 1) != value; });
      rom_bit++;
      search_reads = 0;
    }
  }

  bool read() {
    if (search_mode) {
      assert(rom_bit < 64 && search_reads < 2);
      bool all_one = true;
      for (uint64_t rom : participants) {
        bool bit = (rom >> rom_bit) & 1;
        all_one &= search_reads == 0 ? bit : !bit;
      }
      search_reads++;
      return all_one;
    }
    if (raw_reads.empty()) return true;
    bool value = raw_reads.front();
    raw_reads.pop_front();
    return value;
  }
};
// Separate electrical state for each physical GPIO. The old tests exercised
// several devices on ONE bus; this also catches accidental cross-bus routing.
std::array<Wire, 48> wires;
Wire &wire = wires[29];

void test_lock() {
  assert(wire.lock_depth++ == 0); // no ROM-wide/nested interrupt lock
  wire.lock_start = wire.now;
}
void test_unlock() {
  assert(--wire.lock_depth == 0);
  wire.max_lock = std::max(wire.max_lock, wire.now - wire.lock_start);
}
void k_busy_wait(uint32_t us) {
  for (auto &bus_wire : wires) bus_wire.now += us;
}
void nrf_gpio_cfg(uint32_t pin, int dir, int input, int pull, int drive, int sense) {
  auto &wire = wires.at(pin);
  assert(dir == NRF_GPIO_PIN_DIR_OUTPUT && input == NRF_GPIO_PIN_INPUT_CONNECT);
  assert(drive == NRF_GPIO_PIN_S0D1 && sense == NRF_GPIO_PIN_NOSENSE);
  wire.configured = true;
  wire.configured_pin = pin;
  wire.pull = pull;
}
void nrf_gpio_pin_clear(uint32_t pin) {
  auto &wire = wires.at(pin);
  assert(!wire.low);
  wire.low = true;
  wire.low_start = wire.now;
  wire.read_slot = false;
}
void nrf_gpio_pin_set(uint32_t pin) {
  auto &wire = wires.at(pin);
  if (!wire.low) return;
  wire.low = false;
  unsigned duration = wire.now - wire.low_start;
  wire.pulses.push_back(duration);
  if (duration >= 480) {
    assert(duration <= 960);
    wire.have_reset = true;
    wire.reset_release = wire.now;
    wire.participants = wire.devices;
    wire.search_mode = false;
    wire.command = wire.command_bits = 0;
    wire.rom_bit = wire.search_reads = 0;
  } else if (duration <= 3) {
    assert(duration >= 1);
    wire.read_slot = true;
    wire.slot_value = wire.read();
  } else {
    assert(duration <= 120);
    const bool bit = duration < 15;
    if (!bit) assert(duration >= 60);
    wire.write(bit);
  }
}
uint32_t nrf_gpio_pin_read(uint32_t pin) {
  auto &wire = wires.at(pin);
  if (wire.stuck || wire.low || (wire.stuck_after_reset && wire.have_reset)) return 0;
  if (wire.have_reset && wire.now - wire.reset_release < 480) {
    const auto since_release = wire.now - wire.reset_release;
    return wire.devices.empty() || since_release < 20 || since_release >= 140;
  }
  if (wire.read_slot && wire.now - wire.low_start < 60) {
    unsigned since_edge = wire.now - wire.low_start;
    wire.samples.push_back(since_edge);
    assert(since_edge < 15);
    return wire.slot_value;
  }
  return 1;
}

using esphome::nrf52_one_wire::NRF52OneWireBus;
constexpr uint64_t KNOWN_ROM = 0x9e06050403020128ULL;

uint64_t make_rom(uint64_t body) {
  body &= 0x00ffffffffffffffULL;
  uint8_t bytes[7];
  for (int i = 0; i < 7; ++i) bytes[i] = body >> (8 * i);
  return body | (uint64_t(esphome::crc8(bytes, 7)) << 56);
}

int main() {
  unsigned cases = 0;
  esphome::InternalGPIOPin pin;
  // A synthetic DS18B20 ROM must be discovered and pass the bus CRC check.
  wire = {};
  wire.devices = {KNOWN_ROM};
  NRF52OneWireBus bus;
  bus.set_pin(&pin);
  bus.setup();
  assert(bus.get_devices() == wire.devices);
  assert(wire.pull == NRF_GPIO_PIN_PULLUP);
  assert(wire.configured_pin == 29 && wire.max_lock <= 550);
  assert(!wire.samples.empty());
  ++cases;

  // Branching ROM search across all bit positions, including valid CRC bytes.
  for (int count : {2, 3, 8}) {
    wire = {};
    for (int i = 0; i < count; ++i) wire.devices.push_back(make_rom(0x280028 + (uint64_t(i) << 8)));
    NRF52OneWireBus multi;
    multi.set_pin(&pin);
    multi.setup();
    auto actual = multi.get_devices(), expected = wire.devices;
    std::sort(actual.begin(), actual.end());
    std::sort(expected.begin(), expected.end());
    assert(actual == expected && wire.max_lock <= 550);
    ++cases;
  }

  // Bad ROM CRC must not become a temperature sensor.
  wire = {};
  wire.devices = {KNOWN_ROM ^ (1ULL << 56)};
  NRF52OneWireBus bad_crc;
  bad_crc.set_pin(&pin);
  bad_crc.setup();
  assert(bad_crc.get_devices().empty());
  ++cases;

  // Absent device, stuck LOW, and a false presence caused by a stuck line.
  for (int fault = 0; fault < 3; ++fault) {
    wire = {};
    wire.stuck = fault == 1;
    wire.stuck_after_reset = fault == 2;
    NRF52OneWireBus absent;
    absent.set_pin(&pin);
    absent.setup();
    assert(absent.get_devices().empty());
    auto before = wire.now;
    assert(!absent.skip());
    assert(wire.now - before <= 1000); // bounded failure, no timer underflow
    ++cases;
  }

  // Data transfer: exact LSB order, valid write slots and <15 us read sample.
  wire = {};
  NRF52OneWireBus data;
  data.set_pin(&pin);
  data.setup();
  wire.have_reset = false;
  wire.pulses.clear();
  wire.max_lock = 0;
  data.write8(0xa5);
  assert(wire.pulses == std::vector<unsigned>({6, 60, 6, 60, 60, 6, 60, 6}));
  assert(wire.max_lock <= 60);
  ++cases;
  for (int bit = 0; bit < 64; ++bit) wire.raw_reads.push_back((KNOWN_ROM >> bit) & 1);
  assert(data.read64() == KNOWN_ROM);
  assert(wire.raw_reads.empty());
  ++cases;

  wire = {};
  NRF52OneWireBus port1;
  pin.number = 38;
  port1.set_pin(&pin);
  port1.set_internal_pullup(false);
  port1.setup();
  assert(wires[38].configured_pin == 38 && wires[38].pull == NRF_GPIO_PIN_NOPULL);
  ++cases;
  wire = {};
  NRF52OneWireBus inverted;
  pin.inverted = true;
  inverted.set_pin(&pin);
  inverted.setup();
  assert(inverted.failed && !wire.configured);
  ++cases;

  // Two independent bus instances, in both initialization orders. After
  // initializing B, A must still use its own GPIO, ROM, and input bit stream.
  for (bool reversed : {false, true}) {
    wires = {};
    const uint64_t second_rom = make_rom(0x123456789abc28ULL);
    wires[29].devices = {KNOWN_ROM};
    wires[31].devices = {second_rom};
    esphome::InternalGPIOPin first_pin, second_pin;
    second_pin.number = 31;
    NRF52OneWireBus first, second;
    first.set_pin(&first_pin);
    second.set_pin(&second_pin);
    if (reversed) { second.setup(); first.setup(); }
    else { first.setup(); second.setup(); }
    assert(first.get_devices() == std::vector<uint64_t>{KNOWN_ROM});
    assert(second.get_devices() == std::vector<uint64_t>{second_rom});
    for (int round = 0; round < 3; ++round) {
      assert(first.skip() && second.skip());
      for (int bit = 0; bit < 8; ++bit) {
        wires[29].raw_reads.push_back((0xa5 >> bit) & 1);
        wires[31].raw_reads.push_back((0x3c >> bit) & 1);
      }
      assert(first.read8() == 0xa5);
      assert(second.read8() == 0x3c);
    }
    first.search();
    assert(first.get_devices() == std::vector<uint64_t>{KNOWN_ROM});
    assert(second.get_devices() == std::vector<uint64_t>{second_rom});
    ++cases;
  }

  // An absent or shorted second bus must not prevent discovery on the first.
  for (bool stuck_second : {false, true}) {
    wires = {};
    wires[29].devices = {KNOWN_ROM};
    wires[31].stuck = stuck_second;
    esphome::InternalGPIOPin first_pin, second_pin;
    second_pin.number = 31;
    NRF52OneWireBus first, second;
    first.set_pin(&first_pin);
    second.set_pin(&second_pin);
    second.setup(); first.setup();
    second.diagnose();
    assert(second.get_devices().empty());
    assert(first.get_devices() == std::vector<uint64_t>{KNOWN_ROM});
    assert(first.skip());
    ++cases;
  }

  // Document a current limitation: diagnostic reset detects late presence,
  // but does NOT rerun discovery. Dallas with no startup address stays idle.
  wires = {};
  esphome::InternalGPIOPin late_pin;
  late_pin.number = 31;
  NRF52OneWireBus late;
  late.set_pin(&late_pin);
  late.setup();
  assert(late.get_devices().empty());
  wires[31].devices = {KNOWN_ROM};
  late.diagnose();
  assert(late.get_devices().empty() && late.skip());
  late.search();
  assert(late.get_devices() == std::vector<uint64_t>{KNOWN_ROM});
  ++cases;

  std::printf("PASS: %u scenarios; ROM discovery, CRC, missing/stuck bus, timing, bit order, pin configuration\n", cases);
}
