# Attribution and provenance

## ESPHome-derived runtime code

The ROM-search algorithm in `components/nrf52_one_wire/nrf52_one_wire.cpp` is adapted from ESPHome's GPIO 1-Wire transport. ESPHome's upstream LICENSE assigns C/C++ runtime code to GPLv3, not MIT. The initial local component's incorrect MIT comment was corrected for this publication; the transport logic was not changed.

- Upstream: https://github.com/esphome/esphome
- Reference: https://github.com/esphome/esphome/blob/2026.10.2/esphome/components/gpio/one_wire/gpio_one_wire.cpp
- Copyright (c) 2019 ESPHome
- nRF-specific modifications and repository packaging: copyright (c) 2026 maniekes
- Modification/publication date: 2026-11-10
- License for this project's code: GPL-3.0-only. Upstream notices and terms remain applicable.

## Vendored test dependency

These two files are unmodified copies from ESPHome tag `2026.10.2`:

- `tests/upstream/esphome/components/one_wire/one_wire_bus.cpp`
- `tests/upstream/esphome/components/one_wire/one_wire_bus.h`

Their original licensing document is preserved at `tests/upstream/LICENSE.esphome`. The files are C++ runtime source covered by its GPLv3 section. They are used only by the native tests, not substituted into a user's ESPHome installation.

SHA-256:

```
57e52af979b4090468badf7fc7d88b5f2c7f5bf1d1ea1171d8383616526f5989  one_wire_bus.cpp
69d9dd6ab75a87d1b90c58521a923df16e5ffff9d28c7e1ee770e3335a37ef53  one_wire_bus.h
```

Tests use a synthetic ROM identifier, simulated GPIO/clock/interrupts, and do not contact physical devices. No private Git history is imported.
