#ifdef USE_ESP8266

#include "espnow_hal.h"
#include <espnow.h>
#include <user_interface.h>

namespace esphome {
namespace espnow {

class ESPNowHALESP8266 : public ESPNowHAL {
 public:
  void set_callbacks(std::function<void(const ESPNowRecvInfo &, const uint8_t *, int)> on_data_received,
                     std::function<void(const uint8_t *, bool)> on_data_sent) override {
    on_data_received_ = std::move(on_data_received);
    on_data_sent_ = std::move(on_data_sent);
    instance = this;
  }

  bool init() override {
    static bool initialized = false;
    if (initialized)
      return true;

    if (esp_now_init() != 0)
      return false;
    esp_now_set_self_role(ESP_NOW_ROLE_COMBO);
    esp_now_register_recv_cb(on_data_received_static);
    esp_now_register_send_cb(on_data_sent_static);
    initialized = true;
    return true;
  }

  bool deinit() override {
    // esp_now_deinit is not available on ESP8266
    return true;
  }

  bool add_peer(const uint8_t *peer) override {
    return esp_now_add_peer(const_cast<uint8_t *>(peer), ESP_NOW_ROLE_COMBO, this->get_wifi_channel(), nullptr, 0) == 0;
  }

  bool del_peer(const uint8_t *peer) override { return esp_now_del_peer(const_cast<uint8_t *>(peer)) == 0; }

  bool is_peer_exist(const uint8_t *peer) override { return esp_now_is_peer_exist(const_cast<uint8_t *>(peer)); }

  bool send(const uint8_t *peer_address, const uint8_t *payload, size_t size) override {
    return esp_now_send(const_cast<uint8_t *>(peer_address), const_cast<uint8_t *>(payload), size) == 0;
  }

  bool get_mac_address(uint8_t *mac) override {
    wifi_get_macaddr(STATION_IF, mac);
    return true;
  }

  bool set_wifi_channel(uint8_t channel) override { return wifi_set_channel(channel); }

  uint8_t get_wifi_channel() override { return wifi_get_channel(); }

  void set_wake_window(uint8_t window) override {
    // Not supported on ESP8266
  }

 private:
  static void on_data_received_static(uint8_t *mac_addr, uint8_t *data, uint8_t len) {
    ESPNowRecvInfo recv_info;
    memcpy(recv_info.src_addr, mac_addr, ESPNOW_ETH_ALEN);
    // ESP8266 does not provide destination address, so we'll use broadcast
    memset(recv_info.des_addr, 0xFF, ESPNOW_ETH_ALEN);
    // RSSI and timestamp are not available on ESP8266
    recv_info.rx_ctrl.rssi = 0;
    recv_info.rx_ctrl.timestamp = 0;
    instance->on_data_received_(recv_info, data, len);
  }

  static void on_data_sent_static(uint8_t *mac_addr, uint8_t status) {
    instance->on_data_sent_(mac_addr, status == 0);
  }

  std::function<void(const ESPNowRecvInfo &, const uint8_t *, int)> on_data_received_{};
  std::function<void(const uint8_t *, bool)> on_data_sent_{};
 public:
  static ESPNowHALESP8266 *instance;
};

ESPNowHALESP8266 *ESPNowHALESP8266::instance = nullptr;

ESPNowHAL *get_esp_now_hal() {
  static ESPNowHALESP8266 hal;
  return &hal;
}

}  // namespace espnow
}  // namespace esphome

#endif  // USE_ESP8266
