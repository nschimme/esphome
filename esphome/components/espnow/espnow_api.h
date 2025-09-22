#pragma once

#if defined(USE_ESP32) || defined(USE_ESP8266)

#include "espnow_err.h"
#include "espnow_packet.h"

namespace esphome {
namespace espnow {

#ifdef USE_ESP8266
typedef struct {
    uint8_t peer_addr[ESP_NOW_ETH_ALEN];
    uint8_t lmk[16];
    uint8_t channel;
    uint8_t ifidx;
    bool encrypt;
} esp_now_peer_info_t;
#endif

class ESPNowAPI {
 public:
  virtual ~ESPNowAPI() = default;
  virtual espnow_err_t init(bool wifi_enabled) = 0;
  virtual espnow_err_t deinit() = 0;
  virtual espnow_err_t add_peer(const esp_now_peer_info_t *peer) = 0;
  virtual espnow_err_t del_peer(const uint8_t *peer_addr) = 0;
  virtual espnow_err_t send(const uint8_t *peer_addr, const uint8_t *data, size_t len) = 0;
  virtual espnow_err_t register_recv_cb(void (*cb)(const ESPNowRecvInfo &info, const uint8_t *data, int size), void *arg) = 0;
  virtual espnow_err_t register_send_cb(void (*cb)(const uint8_t *mac_addr, esp_now_send_status_t status), void *arg) = 0;
  virtual uint8_t get_wifi_channel() = 0;
  virtual void set_wifi_channel(uint8_t channel) = 0;
  virtual void get_mac(uint8_t *mac) = 0;
  virtual void get_version(uint32_t &version) = 0;

  static const LogString *espnow_error_to_str(espnow_err_t error);
  static std::string peer_str(uint8_t *peer);
};

#ifdef USE_ESP32
class ESPNowAPI_ESP32 : public ESPNowAPI {
 public:
  espnow_err_t init(bool wifi_enabled) override;
  espnow_err_t deinit() override;
  espnow_err_t add_peer(const esp_now_peer_info_t *peer) override;
  espnow_err_t del_peer(const uint8_t *peer_addr) override;
  espnow_err_t send(const uint8_t *peer_addr, const uint8_t *data, size_t len) override;
  espnow_err_t register_recv_cb(void (*cb)(const ESPNowRecvInfo &info, const uint8_t *data, int size), void *arg) override;
  espnow_err_t register_send_cb(void (*cb)(const uint8_t *mac_addr, esp_now_send_status_t status), void *arg) override;
  uint8_t get_wifi_channel() override;
  void set_wifi_channel(uint8_t channel) override;
  void get_mac(uint8_t *mac) override;
  void get_version(uint32_t &version) override;
};
#endif

#ifdef USE_ESP8266
class ESPNowAPI_ESP8266 : public ESPNowAPI {
 public:
  espnow_err_t init(bool wifi_enabled) override;
  espnow_err_t deinit() override;
  espnow_err_t add_peer(const esp_now_peer_info_t *peer) override;
  espnow_err_t del_peer(const uint8_t *peer_addr) override;
  espnow_err_t send(const uint8_t *peer_addr, const uint8_t *data, size_t len) override;
  espnow_err_t register_recv_cb(void (*cb)(const ESPNowRecvInfo &info, const uint8_t *data, int size), void *arg) override;
  espnow_err_t register_send_cb(void (*cb)(const uint8_t *mac_addr, esp_now_send_status_t status), void *arg) override;
  uint8_t get_wifi_channel() override;
  void set_wifi_channel(uint8_t channel) override;
  void get_mac(uint8_t *mac) override;
  void get_version(uint32_t &version) override;
};
#endif

}  // namespace espnow
}  // namespace esphome

#endif  // defined(USE_ESP32) || defined(USE_ESP8266)
