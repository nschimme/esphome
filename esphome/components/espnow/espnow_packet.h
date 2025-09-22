#pragma once

#if defined(USE_ESP32) || defined(USE_ESP8266)

#include "espnow_err.h"

#include <cstdint>
#include <cstring>
#include <functional>
#include <memory>
#include <vector>

#ifdef USE_ESP32
#include <esp_err.h>
#include <esp_idf_version.h>
#include <esp_now.h>
#endif  // USE_ESP32

#ifdef USE_ESP8266
#include <espnow.h>
#include <ESP8266WiFi.h>
#ifndef ESP_NOW_ETH_ALEN
#define ESP_NOW_ETH_ALEN 6
#endif
#ifndef ESP_NOW_MAX_DATA_LEN
#define ESP_NOW_MAX_DATA_LEN 250
#endif
#endif  // USE_ESP8266

namespace esphome::espnow {

static const uint8_t ESPNOW_BROADCAST_ADDR[ESP_NOW_ETH_ALEN] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
static const uint8_t ESPNOW_MULTICAST_ADDR[ESP_NOW_ETH_ALEN] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFE};

#ifdef USE_ESP8266
struct wifi_pkt_rx_ctrl_t {
    signed rssi:8;
    unsigned rate:4;
    unsigned is_group:1;
    unsigned:1;
    unsigned sig_mode:2;
    unsigned sig_len:12;
    unsigned damatch0:1;
    unsigned damatch1:1;
    unsigned bssidmatch0:1;
    unsigned bssidmatch1:1;
    unsigned MCS:7;
    unsigned CWB:1;
    unsigned HT_length:16;
    unsigned Smoothing:1;
    unsigned Not_Sounding:1;
    unsigned:1;
    unsigned Aggregation:1;
    unsigned STBC:2;
    unsigned FEC_CODING:1;
    unsigned SGI:1;
    unsigned rxend_state:8;
    unsigned ampdu_cnt:8;
    unsigned channel:4;
    unsigned:12;
};
#endif

struct ESPNowRecvInfo {
  uint8_t src_addr[ESP_NOW_ETH_ALEN]; /**< Source address of ESPNOW packet */
  uint8_t des_addr[ESP_NOW_ETH_ALEN]; /**< Destination address of ESPNOW packet */
#ifdef USE_ESP32
  wifi_pkt_rx_ctrl_t *rx_ctrl;        /**< Rx control info of ESPNOW packet */
#else
  wifi_pkt_rx_ctrl_t rx_ctrl;        /**< Rx control info of ESPNOW packet */
#endif
};

using send_callback_t = std::function<void(espnow_err_t)>;

class ESPNowPacket {
 public:
  // NOLINTNEXTLINE(readability-identifier-naming)
  enum esp_now_packet_type_t : uint8_t {
    RECEIVED,
    SENT,
  };

  // Default constructor for pre-allocation in pool
  ESPNowPacket() {}

  void release() {}

  void load_received_data(const ESPNowRecvInfo &info, const uint8_t *data, int size) {
    this->type_ = RECEIVED;
    this->init_received_data_(info, data, size);
  }

  void load_sent_data(const uint8_t *mac_addr, esp_now_send_status_t status) {
    this->type_ = SENT;
    this->init_sent_data_(mac_addr, status);
  }

  // Disable copy to prevent double-delete
  ESPNowPacket(const ESPNowPacket &) = delete;
  ESPNowPacket &operator=(const ESPNowPacket &) = delete;

  union {
    // NOLINTNEXTLINE(readability-identifier-naming)
    struct received_data {
      ESPNowRecvInfo info;                 // Information about the received packet
      uint8_t data[ESP_NOW_MAX_DATA_LEN];  // Data received in the packet
      uint8_t size;                        // Size of the received data
    } receive;

    // NOLINTNEXTLINE(readability-identifier-naming)
    struct sent_data {
      uint8_t address[ESP_NOW_ETH_ALEN];
      esp_now_send_status_t status;
    } sent;
  } packet_;

  esp_now_packet_type_t type_;

  esp_now_packet_type_t type() const { return this->type_; }
  const ESPNowRecvInfo &get_receive_info() const { return this->packet_.receive.info; }

 private:
  void init_received_data_(const ESPNowRecvInfo &info, const uint8_t *data, int size) {
    this->packet_.receive.info = info;
    memcpy(this->packet_.receive.data, data, size);
    this->packet_.receive.size = size;
  }

  void init_sent_data_(const uint8_t *mac_addr, esp_now_send_status_t status) {
    memcpy(this->packet_.sent.address, mac_addr, ESP_NOW_ETH_ALEN);
    this->packet_.sent.status = status;
  }
};

class ESPNowSendPacket {
 public:
  ESPNowSendPacket(const uint8_t *peer_address, const uint8_t *payload, size_t size, const send_callback_t &&callback)
      : callback_(callback) {
    this->init_data_(peer_address, payload, size);
  }
  ESPNowSendPacket(const uint8_t *peer_address, const uint8_t *payload, size_t size) {
    this->init_data_(peer_address, payload, size);
  }

  // Default constructor for pre-allocation in pool
  ESPNowSendPacket() {}

  void release() {}

  // Disable copy to prevent double-delete
  ESPNowSendPacket(const ESPNowSendPacket &) = delete;
  ESPNowSendPacket &operator=(const ESPNowSendPacket &) = delete;

  void load_data(const uint8_t *peer_address, const uint8_t *payload, size_t size, const send_callback_t &callback) {
    this->init_data_(peer_address, payload, size);
    this->callback_ = callback;
  }

  void load_data(const uint8_t *peer_address, const uint8_t *payload, size_t size) {
    this->init_data_(peer_address, payload, size);
    this->callback_ = nullptr;  // Reset callback
  }

  uint8_t address_[ESP_NOW_ETH_ALEN]{0};   // MAC address of the peer to send the packet to
  uint8_t data_[ESP_NOW_MAX_DATA_LEN]{0};  // Data to send
  uint8_t size_{0};                        // Size of the data to send, must be <= ESP_NOW_MAX_DATA_LEN
  send_callback_t callback_{nullptr};      // Callback to call when the send operation is complete

 private:
  void init_data_(const uint8_t *peer_address, const uint8_t *payload, size_t size) {
    memcpy(this->address_, peer_address, ESP_NOW_ETH_ALEN);
    if (size > ESP_NOW_MAX_DATA_LEN) {
      this->size_ = 0;
      return;
    }
    this->size_ = size;
    memcpy(this->data_, payload, this->size_);
  }
};

}  // namespace esphome::espnow

#endif  // defined(USE_ESP32) || defined(USE_ESP8266)
