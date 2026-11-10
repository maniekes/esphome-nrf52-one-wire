#pragma once
#include <cstdint>
enum { NRF_GPIO_PIN_DIR_OUTPUT, NRF_GPIO_PIN_INPUT_CONNECT, NRF_GPIO_PIN_PULLUP,
       NRF_GPIO_PIN_NOPULL, NRF_GPIO_PIN_S0D1, NRF_GPIO_PIN_NOSENSE };
void nrf_gpio_pin_set(uint32_t);
void nrf_gpio_pin_clear(uint32_t);
uint32_t nrf_gpio_pin_read(uint32_t);
void nrf_gpio_cfg(uint32_t, int, int, int, int, int);
