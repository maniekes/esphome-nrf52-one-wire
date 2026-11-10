# ESPHome nRF52 1-Wire

An external ESPHome **1-Wire transport for nRF52840/Zephyr**, using Nordic GPIO and `k_busy_wait()` for short timing-critical intervals. Intended for externally powered DS18B20 probes; the standard ESPHome `dallas_temp` sensor still handles temperature conversion and scratchpad CRC.

**Status: experimental, hardware-specific.** This is not a replacement for every ESPHome 1-Wire platform and is not affiliated with ESPHome. Native simulation tests are included; they do not establish electrical timing or Zigbee coexistence on every board.

## Why a separate transport?

In the reviewed ESPHome 2026.9.1 and 2026.10.2 Zephyr implementations:

- `delayMicroseconds()` calls `k_usleep()`, whose sleep granularity is unsuitable for guaranteeing short 1-Wire slots.
- The GPIO 1-Wire transport calls `ISRInternalGPIOPin::pin_mode()`, but the Zephyr GPIO implementation does not define that method.

This component avoids both dependencies. It configures Nordic `S0D1` open-drain behavior once, keeps the input buffer connected, and uses busy waits in short interrupt-locked sections. ROM discovery and CRC remain integrated with ESPHome's `OneWireBus`; sensor handling remains in `dallas_temp`.

See [design and limitations](docs/design.md) and [upstream attribution](NOTICE.md). A future upstream fix or contribution is possible; this repository does not preclude it.

## Installation

Add the external component to your configuration:

```yaml
external_components:
  - source:
      type: git
      url: https://github.com/maniekes/esphome-nrf52-one-wire
      ref: main
    components: [nrf52_one_wire]

one_wire:
  - platform: nrf52_one_wire
    id: probe_bus
    pin: P0.29
    internal_pullup: true

sensor:
  - platform: dallas_temp
    name: "Probe temperature"
    one_wire_id: probe_bus
    update_interval: 10s
```

`main` is convenient for evaluation. For deployed devices, pin `ref` to a reviewed commit rather than following future changes automatically. See [a complete two-probe example](examples/two-probes.yaml). Changing an external-component source does not itself flash a device.

### Hardware

- Initial target: nRF52840 Pro Micro/SuperMini-style board with a compatible bootloader. ESPHome board profiles and physical board names are not necessarily identical; verify your hardware before using the example profile.
- Pin names are **Nordic port numbers**, not Arduino labels.
- Power each DS18B20 from **3.3 V**, with common GND and a DQ pull-up (typically 4.7 kΩ to 3.3 V). Check signal rise time for your wiring.
- `internal_pullup` defaults to `true`; it is not a guarantee that the internal resistor alone is sufficient for a cable run.
- **Parasite power is not supported**; no strong pull-up is provided.
- Inverted pins and pin numbers outside the nRF52840's 0–47 range are rejected at setup. The transport uses direct Nordic GPIO, not arbitrary GPIO expanders.

## Tests

Requires Bash and a native C++20 compiler. No board, network, ESPHome installation or generated firmware directory is needed:

```sh
bash tests/run.sh
bash tests/run.sh -fsanitize=address,undefined -fno-omit-frame-pointer
```

The suite executes the actual component against simulated wires and a pinned ESPHome ROM-search base. It covers 17 scenarios: discovery, branching ROM search, invalid CRC, missing/stuck buses, bit order, nominal timings, GPIO setup and two independent buses. Fixtures are synthetic.

GPIO operation cost, interrupts, electrical rise time and radio activity are **not** simulated realistically. Compile success and native tests are not hardware acceptance. The GitHub Actions job runs these native tests only; it does not flash hardware or build firmware.

## Compatibility and scope

The transport has been compiled as part of an ESPHome 2026.9.1 / nRF Connect SDK 2.9.2 configuration. Sources were also reviewed against ESPHome 2026.10.2. This is not a promise of compatibility with every release or nRF52 chip; review HAL, GPIO and `OneWireBus` changes when upgrading.

A missing probe during startup may remain unassigned when using automatic address selection. Reset sampling briefly blocks ordinary interrupts; zero-latency interrupts may still preempt timing-critical work. Validate both probes under the intended radio load before relying on measurements in control logic.

This repository intentionally contains no household automations, broker settings, credentials, real device identifiers, compiled firmware or deployment infrastructure.

## License

Project code is distributed under **GPL-3.0-only**; see [LICENSE](LICENSE). ESPHome-derived C++ code retains its upstream GPLv3 terms and attribution. The unmodified upstream licensing document is included with the test fixture. See [NOTICE.md](NOTICE.md) for exact source provenance.
