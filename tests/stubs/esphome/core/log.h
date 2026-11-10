#pragma once
#include <cstdint>
#include <cstdio>
namespace esphome { using LogString = char; }
#define LOG_STR(s) s
#define LOG_STR_ARG(s) s
inline void test_log(const char *, const char *, ...) {}
#define ESP_LOGE(...) test_log(__VA_ARGS__)
#define ESP_LOGW(...) test_log(__VA_ARGS__)
#define ESP_LOGI(...) test_log(__VA_ARGS__)
#define ESP_LOGCONFIG(...) test_log(__VA_ARGS__)
#define LOG_PIN(label, pin) ((void) (pin))
