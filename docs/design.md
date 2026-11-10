# Design and limitations

## Transport boundary

`NRF52OneWireBus` implements ESPHome's `OneWireBus` interface. It performs low-level reset, bit I/O and ROM-search branching. The standard base class checks ROM CRC; `dallas_temp` performs temperature conversion and scratchpad CRC. State is per instance, permitting separate GPIO buses.

The pin is configured once as Nordic S0D1 with its input connected: LOW is driven and HIGH is released. This avoids relying on Zephyr's ISR pin-mode interface or copying a private ISRPinArg layout. Direct hardware access limits portability; most generic GPIO mode options are not used. Configure a non-inverted native nRF52840 pin.

## Nominal timing

| Operation | Requested timing |
|---|---|
| Reset | LOW 480 us; sample presence 70 us after release; recover 410 us |
| Write 1 | LOW 6 us; released 64 us |
| Write 0 | LOW 60 us; released 10 us |
| Read | LOW 3 us; sample after another 10 us; recover 57 us |

These are requested busy-wait durations, not measured waveform timing. GPIO/instruction overhead and interrupts add latency. The read sample has a small margin and must be measured if communication is unreliable.

The IRQ lock covers critical portions of individual bits rather than an entire ROM transaction. Reset still locks ordinary interrupts for approximately 550 us plus overhead. Zephyr zero-latency interrupts may preempt an IRQ lock; coexistence with radio activity requires hardware testing, not assumptions based on this table.

If the bus stays LOW, reset fails after a bounded wait. A LOW line remaining after recovery is not accepted as a valid presence pulse. If a sensor was absent during startup, `diagnose()` does not rediscover or rebind it. Automatic address selection may therefore need a restart after reconnecting a missing sensor. A sensor whose address is already assigned is a different case.

## Why not ordinary GPIO 1-Wire?

Reviewed ESPHome 2026.9.1 and 2026.10.2 use `k_usleep()` for Zephyr `delayMicroseconds()`; the standard GPIO transport also requires an ISR `pin_mode()` definition absent from the reviewed Zephyr GPIO source. These are platform integration issues, not proof that generic ESPHome 1-Wire is broken on other platforms.

Relevant upstream sources:

- https://github.com/esphome/esphome/blob/2026.10.2/esphome/components/zephyr/hal.h
- https://github.com/esphome/esphome/blob/2026.10.2/esphome/components/zephyr/gpio.cpp
- https://github.com/esphome/esphome/blob/2026.10.2/esphome/components/gpio/one_wire/gpio_one_wire.cpp
- https://docs.zephyrproject.org/4.1.0/kernel/services/interrupts.html

Reassess both issues when upgrading. Fixing the upstream platform abstraction or adding a supported transport could eventually replace this external component. No upstream submission has been made by this repository.

## Hardware acceptance checklist

- Verify board profile, bootloader, port labels and power supply.
- Check external pull-up and DQ rise time for the actual cable.
- Test both probes concurrently, including while Zigbee traffic is active.
- Observe CRC errors, missing readings and radio connectivity over an extended run.
- Test startup with a missing/late-powered probe and a fault on one bus.
- Do not rely on this component alone for heater overtemperature protection.
