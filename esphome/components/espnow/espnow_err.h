#pragma once

#if defined(USE_ESP32) || defined(USE_ESP8266)

#ifdef USE_ESP32
#include <esp_err.h>
#include <esp_now.h>
#endif  // USE_ESP32

namespace esphome::espnow {

#ifdef USE_ESP32
using espnow_err_t = esp_err_t;
#else
using espnow_err_t = int;
#endif

#ifdef USE_ESP32
static const esp_err_t ESP_ERR_ESPNOW_CMP_BASE = (ESP_ERR_ESPNOW_BASE + 20);
#else
constexpr espnow_err_t ESP_OK = 0;
constexpr espnow_err_t ESP_FAIL = -1;
constexpr espnow_err_t ESP_ERR_ESPNOW_BASE = 0x3000;
constexpr espnow_err_t ESP_ERR_ESPNOW_NOT_INIT = ESP_ERR_ESPNOW_BASE + 1;
constexpr espnow_err_t ESP_ERR_ESPNOW_ARG = ESP_ERR_ESPNOW_BASE + 2;
constexpr espnow_err_t ESP_ERR_ESPNOW_NO_MEM = ESP_ERR_ESPNOW_BASE + 3;
constexpr espnow_err_t ESP_ERR_ESPNOW_INTERNAL = ESP_ERR_ESPNOW_BASE + 4;
constexpr espnow_err_t ESP_ERR_ESPNOW_IF = ESP_ERR_ESPNOW_BASE + 5;
constexpr espnow_err_t ESP_ERR_ESPNOW_NOT_FOUND = ESP_ERR_ESPNOW_BASE + 6;
static const espnow_err_t ESP_ERR_ESPNOW_CMP_BASE = (ESP_ERR_ESPNOW_BASE + 20);
#endif

static const espnow_err_t ESP_ERR_ESPNOW_FAILED = (ESP_ERR_ESPNOW_CMP_BASE + 1);
static const espnow_err_t ESP_ERR_ESPNOW_OWN_ADDRESS = (ESP_ERR_ESPNOW_CMP_BASE + 2);
static const espnow_err_t ESP_ERR_ESPNOW_DATA_SIZE = (ESP_ERR_ESPNOW_CMP_BASE + 3);
static const espnow_err_t ESP_ERR_ESPNOW_PEER_NOT_SET = (ESP_ERR_ESPNOW_CMP_BASE + 4);
static const espnow_err_t ESP_ERR_ESPNOW_PEER_NOT_PAIRED = (ESP_ERR_ESPNOW_CMP_BASE + 5);

#ifdef USE_ESP8266
// from esp_now.h
enum esp_now_send_status_t {
  ESP_NOW_SEND_SUCCESS,
  ESP_NOW_SEND_FAIL,
};
#endif

}  // namespace esphome::espnow

#endif  // defined(USE_ESP32) || defined(USE_ESP8266)
