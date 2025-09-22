#ifdef USE_ESP32

#include "espnow_hal.h"
#include <esp_event.h>
#include <esp_now.h>
#include <esp_wifi.h>

namespace esphome {
namespace espnow {

class ESPNowHALESP32 : public ESPNowHAL {
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

    if (esp_now_init() != ESP_OK)
      return false;
    esp_now_register_recv_cb(on_data_received_static);
    esp_now_register_send_cb(on_data_sent_static);
    initialized = true;
    return true;
  }

  bool deinit() override { return esp_now_deinit() == ESP_OK; }

  bool add_peer(const uint8_t *peer) override {
    esp_now_peer_info_t peer_info{};
    peer_info.ifidx = WIFI_IF_STA;
    memcpy(peer_info.peer_addr, peer, ESPNOW_ETH_ALEN);
    return esp_now_add_peer(&peer_info) == ESP_OK;
  }

  bool del_peer(const uint8_t *peer) override { return esp_now_del_peer(peer) == ESP_OK; }

  bool is_peer_exist(const uint8_t *peer) override { return esp_now_is_peer_exist(peer); }

  bool send(const uint8_t *peer_address, const uint8_t *payload, size_t size) override {
    return esp_now_send(peer_address, payload, size) == ESP_OK;
  }

  bool get_mac_address(uint8_t *mac) override { return esp_wifi_get_mac(WIFI_IF_STA, mac) == ESP_OK; }

  bool set_wifi_channel(uint8_t channel) override {
    return esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE) == ESP_OK;
  }

  uint8_t get_wifi_channel() override {
    uint8_t channel;
    wifi_second_chan_t dummy;
    esp_wifi_get_channel(&channel, &dummy);
    return channel;
  }

  void set_wake_window(uint8_t window) override {
#ifdef USE_DEEP_SLEEP
    esp_now_set_wake_window(window);
#endif
  }

 private:
  static void on_data_received_static(const esp_now_recv_info_t *info, const uint8_t *data, int size) {
    ESPNowRecvInfo recv_info;
    memcpy(recv_info.src_addr, info->src_addr, ESPNOW_ETH_ALEN);
    memcpy(recv_info.des_addr, info->des_addr, ESPNOW_ETH_ALEN);
    recv_info.rx_ctrl.rssi = info->rx_ctrl->rssi;
    recv_info.rx_ctrl.timestamp = info->rx_ctrl->timestamp;
    instance->on_data_received_(recv_info, data, size);
  }

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 5, 0)
  static void on_data_sent_static(const esp_now_send_info_t *info, esp_now_send_status_t status) {
    instance->on_data_sent_(info->des_addr, status == ESP_NOW_SEND_SUCCESS);
  }
#else
  static void on_data_sent_static(const uint8_t *mac_addr, esp_now_send_status_t status) {
    instance->on_data_sent_(mac_addr, status == ESP_NOW_SEND_SUCCESS);
  }
#endif

  std::function<void(const ESPNowRecvInfo &, const uint8_t *, int)> on_data_received_{};
  std::function<void(const uint8_t *, bool)> on_data_sent_{};
  static ESPNowHALESP32 *instance;
};

ESPNowHALESP32 *ESPNowHALESP32::instance = nullptr;

ESPNowHAL *get_esp_now_hal() {
  static ESPNowHALESP32 hal;
  return &hal;
}

}  // namespace espnow
}  // namespace esphome

#endif  // USE_ESP32
