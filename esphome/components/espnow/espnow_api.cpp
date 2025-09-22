#include "espnow_api.h"

#if defined(USE_ESP32) || defined(USE_ESP8266)

#include "esphome/core/log.h"
#include "esphome/core/helpers.h"

namespace esphome {
namespace espnow {

const LogString *ESPNowAPI::espnow_error_to_str(espnow_err_t error) {
  switch (error) {
    case ESP_ERR_ESPNOW_FAILED:
      return LOG_STR("ESPNow is in fail mode");
    case ESP_ERR_ESPNOW_OWN_ADDRESS:
      return LOG_STR("Message to your self");
    case ESP_ERR_ESPNOW_DATA_SIZE:
      return LOG_STR("Data size to large");
    case ESP_ERR_ESPNOW_PEER_NOT_SET:
      return LOG_STR("Peer address not set");
    case ESP_ERR_ESPNOW_PEER_NOT_PAIRED:
      return LOG_STR("Peer address not paired");
    case ESP_ERR_ESPNOW_NOT_INIT:
      return LOG_STR("Not init");
    case ESP_ERR_ESPNOW_ARG:
      return LOG_STR("Invalid argument");
    case ESP_ERR_ESPNOW_INTERNAL:
      return LOG_STR("Internal Error");
    case ESP_ERR_ESPNOW_NO_MEM:
      return LOG_STR("Our of memory");
    case ESP_ERR_ESPNOW_NOT_FOUND:
      return LOG_STR("Peer not found");
    case ESP_ERR_ESPNOW_IF:
      return LOG_STR("Interface does not match");
    case ESP_OK:
      return LOG_STR("OK");
    case ESP_FAIL:
      return LOG_STR("Failed");
    default:
      return LOG_STR("Unknown Error");
  }
}

std::string ESPNowAPI::peer_str(uint8_t *peer) {
  if (peer == nullptr || peer[0] == 0) {
    return "[Not Set]";
  } else if (memcmp(peer, ESPNOW_BROADCAST_ADDR, ESP_NOW_ETH_ALEN) == 0) {
    return "[Broadcast]";
#ifdef USE_ESP32
  } else if (memcmp(peer, ESPNOW_MULTICAST_ADDR, ESP_NOW_ETH_ALEN) == 0) {
    return "[Multicast]";
#endif
  } else {
    return format_mac_address_pretty(peer);
  }
}

#ifdef USE_ESP32
static void (*recv_cb_esp32)(const ESPNowRecvInfo &info, const uint8_t *data, int size) = nullptr;
static void *recv_cb_arg_esp32 = nullptr;
static void (*send_cb_esp32)(const uint8_t *mac_addr, esp_now_send_status_t status) = nullptr;
static void *send_cb_arg_esp32 = nullptr;

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 5, 0)
void on_send_report_esp32(const esp_now_send_info_t *info, esp_now_send_status_t status) {
  if (send_cb_esp32 != nullptr) {
    send_cb_esp32(info->des_addr, status);
  }
}
#else
void on_send_report_esp32(const uint8_t *mac_addr, esp_now_send_status_t status) {
  if (send_cb_esp32 != nullptr) {
    send_cb_esp32(mac_addr, status);
  }
}
#endif

void on_data_received_esp32(const esp_now_recv_info_t *info, const uint8_t *data, int size) {
  if (recv_cb_esp32 != nullptr) {
    ESPNowRecvInfo recv_info;
    memcpy(recv_info.src_addr, info->src_addr, ESP_NOW_ETH_ALEN);
    memcpy(recv_info.des_addr, info->des_addr, ESP_NOW_ETH_ALEN);
    recv_info.rx_ctrl.rssi = info->rx_ctrl->rssi;
    recv_info.rx_ctrl.timestamp = info->rx_ctrl->timestamp;
    recv_cb_esp32(recv_info, data, size);
  }
}

espnow_err_t ESPNowAPI_ESP32::init() {
  esp_event_loop_create_default();
  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  esp_wifi_init(&cfg);
  esp_wifi_set_mode(WIFI_MODE_STA);
  esp_wifi_set_storage(WIFI_STORAGE_RAM);
  esp_wifi_set_ps(WIFI_PS_NONE);
  esp_wifi_start();
  esp_wifi_disconnect();
  return esp_now_init();
}
espnow_err_t ESPNowAPI_ESP32::deinit() { return esp_now_deinit(); }
espnow_err_t ESPNowAPI_ESP32::add_peer(const esp_now_peer_info_t *peer) {
  return esp_now_add_peer(peer);
}
espnow_err_t ESPNowAPI_ESP32::del_peer(const uint8_t *peer_addr) { return esp_now_del_peer(peer_addr); }
espnow_err_t ESPNowAPI_ESP32::send(const uint8_t *peer_addr, const uint8_t *data, size_t len) {
  return esp_now_send(peer_addr, data, len);
}
espnow_err_t ESPNowAPI_ESP32::register_recv_cb(void (*cb)(const ESPNowRecvInfo &info, const uint8_t *data, int size),
                                             void *arg) {
  recv_cb_esp32 = cb;
  recv_cb_arg_esp32 = arg;
  return esp_now_register_recv_cb(on_data_received_esp32);
}
espnow_err_t ESPNowAPI_ESP32::register_send_cb(void (*cb)(const uint8_t *mac_addr, esp_now_send_status_t status),
                                             void *arg) {
  send_cb_esp32 = cb;
  send_cb_arg_esp32 = arg;
  return esp_now_register_send_cb(on_send_report_esp32);
}
uint8_t ESPNowAPI_ESP32::get_wifi_channel() {
  uint8_t channel;
  wifi_second_chan_t second;
  esp_wifi_get_channel(&channel, &second);
  return channel;
}
void ESPNowAPI_ESP32::set_wifi_channel(uint8_t channel) {
  esp_wifi_set_promiscuous(true);
  esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE);
  esp_wifi_set_promiscuous(false);
}
void ESPNowAPI_ESP32::get_mac(uint8_t *mac) { esp_wifi_get_mac(WIFI_IF_STA, mac); }
void ESPNowAPI_ESP32::get_version(uint32_t &version) { esp_now_get_version(&version); }
#endif

#ifdef USE_ESP8266
static void (*recv_cb_esp8266)(const ESPNowRecvInfo &info, const uint8_t *data, int size) = nullptr;
static void *recv_cb_arg_esp8266 = nullptr;
static void (*send_cb_esp8266)(const uint8_t *mac_addr, esp_now_send_status_t status) = nullptr;
static void *send_cb_arg_esp8266 = nullptr;
static int8_t last_rssi = 0;

struct ieee80211_frame {
    uint8_t frame_control[2];
    uint8_t duration_id[2];
    uint8_t addr1[6];
    uint8_t addr2[6];
    uint8_t addr3[6];
    uint8_t seq_ctrl[2];
    uint8_t addr4[6];
    uint8_t payload[0];
};

struct wifi_promiscuous_pkt_t {
    wifi_pkt_rx_ctrl_t rx_ctrl;
    uint8_t payload[0];
};

void promiscuous_rx_cb(uint8_t *buf, uint16_t len) {
  if (len < sizeof(wifi_promiscuous_pkt_t) + sizeof(ieee80211_frame)) {
    return;
  }
  wifi_promiscuous_pkt_t *pkt = (wifi_promiscuous_pkt_t *) buf;
  ieee80211_frame *frame = (ieee80211_frame *) pkt->payload;
  // check for espnow packet
  if (frame->frame_control[0] == 0xd0 && frame->frame_control[1] == 0x00) {
    last_rssi = pkt->rx_ctrl.rssi;
  }
}

void on_data_received_esp8266(uint8_t *mac_addr, uint8_t *data, uint8_t len) {
  if (recv_cb_esp8266 != nullptr) {
    ESPNowRecvInfo recv_info;
    memcpy(recv_info.src_addr, mac_addr, ESP_NOW_ETH_ALEN);
    memcpy(recv_info.des_addr, ESPNOW_BROADCAST_ADDR, ESP_NOW_ETH_ALEN);
    recv_info.rx_ctrl.rssi = last_rssi;
    recv_cb_esp8266(recv_info, data, len);
  }
}

void on_send_report_esp8266(uint8_t *mac_addr, uint8_t status) {
  if (send_cb_esp8266 != nullptr) {
    send_cb_esp8266(mac_addr, (esp_now_send_status_t) status);
  }
}

espnow_err_t ESPNowAPI_ESP8266::init() {
  wifi_set_promiscuous_rx_cb(promiscuous_rx_cb);
  wifi_set_promiscuous(true);
  return esp_now_init();
}
espnow_err_t ESPNowAPI_ESP8266::deinit() {
  wifi_set_promiscuous(false);
  // esp_now_unregister_recv_cb and esp_now_unregister_send_cb are not available on ESP8266
  // esp_now_deinit will unregister them
  esp_now_deinit();
  return ESP_OK;
}
espnow_err_t ESPNowAPI_ESP8266::add_peer(const esp_now_peer_info_t *peer) {
  return esp_now_add_peer(const_cast<uint8_t *>(peer->peer_addr), ESP_NOW_ROLE_COMBO, peer->channel,
                          peer->encrypt ? const_cast<uint8_t *>(peer->lmk) : nullptr, peer->encrypt ? 16 : 0);
}
espnow_err_t ESPNowAPI_ESP8266::del_peer(const uint8_t *peer_addr) {
  return esp_now_del_peer(const_cast<uint8_t *>(peer_addr));
}
espnow_err_t ESPNowAPI_ESP8266::send(const uint8_t *peer_addr, const uint8_t *data, size_t len) {
  return esp_now_send(const_cast<uint8_t *>(peer_addr), const_cast<uint8_t *>(data), len);
}
espnow_err_t ESPNowAPI_ESP8266::register_recv_cb(void (*cb)(const ESPNowRecvInfo &info, const uint8_t *data, int size),
                                               void *arg) {
  recv_cb_esp8266 = cb;
  recv_cb_arg_esp8266 = arg;
  return esp_now_register_recv_cb(on_data_received_esp8266);
}
espnow_err_t ESPNowAPI_ESP8266::register_send_cb(void (*cb)(const uint8_t *mac_addr, esp_now_send_status_t status),
                                               void *arg) {
  send_cb_esp8266 = cb;
  send_cb_arg_esp8266 = arg;
  return esp_now_register_send_cb(on_send_report_esp8266);
}
uint8_t ESPNowAPI_ESP8266::get_wifi_channel() { return wifi_get_channel(); }
void ESPNowAPI_ESP8266::set_wifi_channel(uint8_t channel) {
  // esp_wifi_set_promiscuous is not available on ESP8266
  wifi_set_channel(channel);
}
void ESPNowAPI_ESP8266::get_mac(uint8_t *mac) {
  // esp_wifi_get_mac is not available on ESP8266, use WiFi.macAddress()
  WiFi.macAddress(mac);
}
void ESPNowAPI_ESP8266::get_version(uint32_t &version) {
  // esp_now_get_version is not available on ESP8266
  version = 0;
}
#endif

}  // namespace espnow
}  // namespace esphome

#endif  // defined(USE_ESP32) || defined(USE_ESP8266)
