#pragma once
#include <cstdint>
void k_busy_wait(uint32_t us);
void k_usleep(uint32_t) = delete;
