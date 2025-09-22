#pragma once

#include "esphome/core/helpers.h"
#include <cstdint>
#include <cstring>
#include <functional>
#include <memory>
#include <vector>

namespace esphome {
namespace espnow {

const uint8_t ESPNOW_ETH_ALEN = 6;
const uint8_t ESPNOW_MAX_DATA_LEN = 250;

static const uint8_t ESPNOW_BROADCAST_ADDR[ESPNOW_ETH_ALEN] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
static const uint8_t ESPNOW_MULTICAST_ADDR[ESPNOW_ETH_ALEN] = {0x01, 0x00, 0x5E, 0x00, 0x00, 0x00};

struct WifiPacketRxControl {
  int8_t rssi;
  uint32_t timestamp;
};

struct ESPNowRecvInfo {
  uint8_t src_addr[ESPNOW_ETH_ALEN];
  uint8_t des_addr[ESPNOW_ETH_ALEN];
  WifiPacketRxControl rx_ctrl;
};

using send_callback_t = std::function<void(bool)>;

class ESPNowPacket {
 public:
  enum esp_now_packet_type_t : uint8_t {
    RECEIVED,
    SENT,
  };

  ESPNowPacket(const ESPNowRecvInfo &info, const uint8_t *data, int size) {
    this->init_received_data_(info, data, size);
  }

  ESPNowPacket(const uint8_t *mac_addr, bool success) { this->init_sent_data_(mac_addr, success); }

  ESPNowPacket() = default;

  void release() {}

  void load_received_data(const ESPNowRecvInfo &info, const uint8_t *data, int size) {
    this->type_ = RECEIVED;
    this->init_received_data_(info, data, size);
  }

  void load_sent_data(const uint8_t *mac_addr, bool success) {
    this->type_ = SENT;
    this->init_sent_data_(mac_addr, success);
  }

  ESPNowPacket(const ESPNowPacket &) = delete;
  ESPNowPacket &operator=(const ESPNowPacket &) = delete;

  union {
    struct received_data {
      ESPNowRecvInfo info;
      uint8_t data[ESPNOW_MAX_DATA_LEN];
      uint8_t size;
    } receive;

    struct sent_data {
      uint8_t address[ESPNOW_ETH_ALEN];
      bool success;
    } sent;
  } packet_{};

  esp_now_packet_type_t type_{};

  const ESPNowRecvInfo &get_receive_info() const { return this->packet_.receive.info; }

 private:
  void init_received_data_(const ESPNowRecvInfo &info, const uint8_t *data, int size) {
    this->packet_.receive.info = info;
    memcpy(this->packet_.receive.data, data, size);
    this->packet_.receive.size = size;
  }

  void init_sent_data_(const uint8_t *mac_addr, bool success) {
    memcpy(this->packet_.sent.address, mac_addr, ESPNOW_ETH_ALEN);
    this->packet_.sent.success = success;
  }
};

class ESPNowSendPacket {
 public:
  ESPNowSendPacket(const uint8_t *peer_address, const uint8_t *payload, size_t size, send_callback_t &&callback)
      : callback_(std::move(callback)) {
    this->init_data_(peer_address, payload, size);
  }

  ESPNowSendPacket() = default;

  void release() {}

  ESPNowSendPacket(const ESPNowSendPacket &) = delete;
  ESPNowSendPacket &operator=(const ESPNowSendPacket &) = delete;

  void load_data(const uint8_t *peer_address, const uint8_t *payload, size_t size, send_callback_t callback) {
    this->init_data_(peer_address, payload, size);
    this->callback_ = std::move(callback);
  }

  uint8_t address_[ESPNOW_ETH_ALEN]{};
  uint8_t data_[ESPNOW_MAX_DATA_LEN]{};
  uint8_t size_{};
  send_callback_t callback_{nullptr};

 private:
  void init_data_(const uint8_t *peer_address, const uint8_t *payload, size_t size) {
    memcpy(this->address_, peer_address, ESPNOW_ETH_ALEN);
    if (size > ESPNOW_MAX_DATA_LEN) {
      this->size_ = 0;
      return;
    }
    this->size_ = size;
    memcpy(this->data_, payload, this->size_);
  }
};

class ESPNowHAL {
 public:
  virtual void set_callbacks(std::function<void(const ESPNowRecvInfo &, const uint8_t *, int)> on_data_received,
                           std::function<void(const uint8_t *, bool)> on_data_sent) = 0;
  virtual bool init() = 0;
  virtual bool deinit() = 0;
  virtual bool add_peer(const uint8_t *peer) = 0;
  virtual bool del_peer(const uint8_t *peer) = 0;
  virtual bool is_peer_exist(const uint8_t *peer) = 0;
  virtual bool send(const uint8_t *peer_address, const uint8_t *payload, size_t size) = 0;
  virtual bool get_mac_address(uint8_t *mac) = 0;
  virtual bool set_wifi_channel(uint8_t channel) = 0;
  virtual uint8_t get_wifi_channel() = 0;
  virtual void set_wake_window(uint8_t window) = 0;
};

ESPNowHAL *get_esp_now_hal();

}  // namespace espnow
}  // namespace esphome
