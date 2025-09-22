#include "espnow_component.h"

#if defined(USE_ESP32) || defined(USE_ESP8266)

#include "esphome/core/log.h"

#ifdef USE_WIFI
#include "esphome/components/wifi/wifi_component.h"
#endif

namespace esphome {
namespace espnow {

static constexpr const char *TAG = "espnow";
ESPNowComponent *global_esp_now = nullptr;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

static const LogString *espnow_error_to_str(espnow_err_t error) {
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

std::string peer_str(uint8_t *peer) {
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

bool ESPNowComponent::is_peer_exist(const uint8_t *peer_addr) {
  for (auto &it : this->peers_) {
    if (memcmp(it.address, peer_addr, ESP_NOW_ETH_ALEN) == 0) {
      return true;
    }
  }
  return false;
}

void ESPNowComponent::on_data_received(const ESPNowRecvInfo &info, const uint8_t *data, int size) {
  ESPNowPacket *packet = this->receive_packet_pool_.allocate();
  if (packet == nullptr) {
    this->receive_packet_queue_.increment_dropped_count();
    return;
  }

  packet->load_received_data(info, data, size);
  this->receive_packet_queue_.push(packet);
}

void ESPNowComponent::on_send_report(const uint8_t *mac_addr, esp_now_send_status_t status) {
  ESPNowPacket *packet = this->receive_packet_pool_.allocate();
  if (packet == nullptr) {
    this->receive_packet_queue_.increment_dropped_count();
    return;
  }

  packet->load_sent_data(mac_addr, status);
  this->receive_packet_queue_.push(packet);
}

ESPNowComponent::ESPNowComponent() {
  global_esp_now = this;
#ifdef USE_ESP32
  this->api_ = new ESPNowAPI_ESP32();
#elif defined(USE_ESP8266)
  this->api_ = new ESPNowAPI_ESP8266();
#endif
}

void ESPNowComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "espnow:");
  if (this->is_disabled()) {
    ESP_LOGCONFIG(TAG, "  Disabled");
    return;
  }
  uint8_t mac[6];
  this->api_->get_mac(mac);
  ESP_LOGCONFIG(TAG,
                "  Own address: %s
"
                "  Wi-Fi channel: %d",
                format_mac_address_pretty(mac).c_str(), this->wifi_channel_);
#ifdef USE_WIFI
  ESP_LOGCONFIG(TAG, "  Wi-Fi enabled: %s", YESNO(this->is_wifi_enabled()));
#endif
}

bool ESPNowComponent::is_wifi_enabled() {
#ifdef USE_WIFI
  return wifi::global_wifi_component != nullptr && !wifi::global_wifi_component->is_disabled();
#else
  return false;
#endif
}

void ESPNowComponent::setup() {
  if (this->enable_on_boot_) {
    this->enable_();
  } else {
    this->state_ = ESPNOW_STATE_DISABLED;
  }
}

void ESPNowComponent::enable() {
  if (this->state_ == ESPNOW_STATE_ENABLED)
    return;

  ESP_LOGD(TAG, "Enabling");
  this->state_ = ESPNOW_STATE_OFF;

  this->enable_();
}

void ESPNowComponent::enable_() {
  if (!this->is_wifi_enabled()) {
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
  }
  this->wifi_channel_ = this->api_->get_wifi_channel();

  if (this->api_->init() != ESP_OK) {
    ESP_LOGE(TAG, "esp_now_init failed");
    this->mark_failed();
    return;
  }

  if (this->api_->register_recv_cb([](const ESPNowRecvInfo &info, const uint8_t *data, int size) {
    global_esp_now->on_data_received(info, data, size);
  }, this) != ESP_OK) {
    ESP_LOGE(TAG, "esp_now_register_recv_cb failed");
    this->mark_failed();
    return;
  }

  if (this->api_->register_send_cb([](const uint8_t *mac_addr, esp_now_send_status_t status) {
    global_esp_now->on_send_report(mac_addr, status);
  }, this) != ESP_OK) {
    ESP_LOGE(TAG, "esp_now_register_send_cb failed");
    this->mark_failed();
    return;
  }

  this->api_->get_mac(this->own_address_);

  this->state_ = ESPNOW_STATE_ENABLED;

  for (auto peer : this->peers_) {
    this->add_peer(peer.address);
  }
}

void ESPNowComponent::disable() {
  if (this->state_ == ESPNOW_STATE_DISABLED)
    return;

  ESP_LOGD(TAG, "Disabling");
  this->state_ = ESPNOW_STATE_DISABLED;

  this->api_->deinit();
}

void ESPNowComponent::apply_wifi_channel() {
  if (this->state_ == ESPNOW_STATE_DISABLED) {
    ESP_LOGE(TAG, "Cannot set channel when ESPNOW disabled");
    this->mark_failed();
    return;
  }

  if (this->is_wifi_enabled()) {
    ESP_LOGE(TAG, "Cannot set channel when Wi-Fi enabled");
    this->mark_failed();
    return;
  }

  ESP_LOGI(TAG, "Channel set to %d.", this->wifi_channel_);
  this->api_->set_wifi_channel(this->wifi_channel_);
}

void ESPNowComponent::loop() {
#ifdef USE_WIFI
  if (wifi::global_wifi_component != nullptr && wifi::global_wifi_component->is_connected()) {
    int32_t new_channel = this->api_->get_wifi_channel();
    if (new_channel != this->wifi_channel_) {
      ESP_LOGI(TAG, "Wifi Channel is changed from %d to %d.", this->wifi_channel_, new_channel);
      this->wifi_channel_ = new_channel;
    }
  }
#endif
  // Process received packets
  ESPNowPacket *packet = this->receive_packet_queue_.pop();
  while (packet != nullptr) {
    switch (packet->type_) {
      case ESPNowPacket::RECEIVED: {
        const ESPNowRecvInfo info = packet->get_receive_info();
        bool is_peer_exist = this->is_peer_exist(info.src_addr);
        if (!is_peer_exist) {
          bool handled = false;
          for (auto *handler : this->unknown_peer_handlers_) {
            if (handler->on_unknown_peer(info, packet->packet_.receive.data, packet->packet_.receive.size)) {
              handled = true;
              break;  // If a handler returns true, stop processing further handlers
            }
          }
          if (!handled && this->auto_add_peer_) {
            this->add_peer(info.src_addr);
          }
        }
        // Intentionally left as if instead of else in case the peer is added above
        if (is_peer_exist) {
#if ESPHOME_LOG_LEVEL >= ESPHOME_LOG_LEVEL_VERBOSE
          ESP_LOGV(TAG, "<<< [%s -> %s] %s", format_mac_address_pretty(info.src_addr).c_str(),
                   format_mac_address_pretty(info.des_addr).c_str(),
                   format_hex_pretty(packet->packet_.receive.data, packet->packet_.receive.size).c_str());
#endif
          if (memcmp(info.des_addr, ESPNOW_BROADCAST_ADDR, ESP_NOW_ETH_ALEN) == 0) {
            for (auto *handler : this->broadcasted_handlers_) {
              if (handler->on_broadcasted(info, packet->packet_.receive.data, packet->packet_.receive.size))
                break;  // If a handler returns true, stop processing further handlers
            }
          } else {
            for (auto *handler : this->received_handlers_) {
              if (handler->on_received(info, packet->packet_.receive.data, packet->packet_.receive.size))
                break;  // If a handler returns true, stop processing further handlers
            }
          }
        }
        break;
      }
      case ESPNowPacket::SENT: {
#if ESPHOME_LOG_LEVEL >= ESPHOME_LOG_LEVEL_VERBOSE
        ESP_LOGV(TAG, ">>> [%s] %s", format_mac_address_pretty(packet->packet_.sent.address).c_str(),
                 LOG_STR_ARG(espnow_error_to_str(packet->packet_.sent.status)));
#endif
        if (this->current_send_packet_ != nullptr) {
          this->current_send_packet_->callback_(packet->packet_.sent.status);
          this->send_packet_pool_.release(this->current_send_packet_);
          this->current_send_packet_ = nullptr;  // Reset current packet after sending
        }
        break;
      }
      default:
        break;
    }
    // Return the packet to the pool
    this->receive_packet_pool_.release(packet);
    packet = this->receive_packet_queue_.pop();
  }

  // Process sending packet queue
  if (this->current_send_packet_ == nullptr) {
    this->send_();
  }

  // Log dropped received packets periodically
  uint16_t received_dropped = this->receive_packet_queue_.get_and_reset_dropped_count();
  if (received_dropped > 0) {
    ESP_LOGW(TAG, "Dropped %u received packets due to buffer overflow", received_dropped);
  }

  // Log dropped send packets periodically
  uint16_t send_dropped = this->send_packet_queue_.get_and_reset_dropped_count();
  if (send_dropped > 0) {
    ESP_LOGW(TAG, "Dropped %u send packets due to buffer overflow", send_dropped);
  }
}

uint8_t ESPNowComponent::get_wifi_channel() { return this->api_->get_wifi_channel(); }

espnow_err_t ESPNowComponent::send(const uint8_t *peer_address, const uint8_t *payload, size_t size,
                                const send_callback_t &callback) {
  if (this->state_ != ESPNOW_STATE_ENABLED) {
    return ESP_ERR_ESPNOW_NOT_INIT;
  } else if (this->is_failed()) {
    return ESP_ERR_ESPNOW_FAILED;
  } else if (peer_address == 0ULL) {
    return ESP_ERR_ESPNOW_PEER_NOT_SET;
  } else if (memcmp(peer_address, this->own_address_, ESP_NOW_ETH_ALEN) == 0) {
    return ESP_ERR_ESPNOW_OWN_ADDRESS;
  } else if (size > ESP_NOW_MAX_DATA_LEN) {
    return ESP_ERR_ESPNOW_DATA_SIZE;
  }

  bool is_peer_exist = this->is_peer_exist(peer_address);

  if (!is_peer_exist) {
    if (memcmp(peer_address, ESPNOW_BROADCAST_ADDR, ESP_NOW_ETH_ALEN) == 0 || this->auto_add_peer_) {
      espnow_err_t err = this->add_peer(peer_address);
      if (err != ESP_OK) {
        return err;
      }
    } else {
      return ESP_ERR_ESPNOW_PEER_NOT_PAIRED;
    }
  }
  // Allocate a packet from the pool
  ESPNowSendPacket *packet = this->send_packet_pool_.allocate();
  if (packet == nullptr) {
    this->send_packet_queue_.increment_dropped_count();
    ESP_LOGE(TAG, "Failed to allocate send packet from pool");
    this->status_momentary_warning("send-packet-pool-full");
    return ESP_ERR_ESPNOW_NO_MEM;
  }
  // Load the packet data
  packet->load_data(peer_address, payload, size, callback);
  // Push the packet to the send queue
  this->send_packet_queue_.push(packet);
  return ESP_OK;
}

void ESPNowComponent::send_() {
  ESPNowSendPacket *packet = this->send_packet_queue_.pop();
  if (packet == nullptr) {
    return;  // No packets to send
  }

  this->current_send_packet_ = packet;
  espnow_err_t err = this->api_->send(packet->address_, packet->data_, packet->size_);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Failed to send packet to %s", format_mac_address_pretty(packet->address_).c_str());
    if (packet->callback_ != nullptr) {
      packet->callback_(err);
    }
    this->status_momentary_warning("send-failed");
    this->send_packet_pool_.release(packet);
    this->current_send_packet_ = nullptr;  // Reset current packet
    return;
  }
}

espnow_err_t ESPNowComponent::add_peer(const uint8_t *peer) {
  if (this->state_ != ESPNOW_STATE_ENABLED || this->is_failed()) {
    return ESP_ERR_ESPNOW_NOT_INIT;
  }

  if (memcmp(peer, this->own_address_, ESP_NOW_ETH_ALEN) == 0) {
    this->status_momentary_warning("peer-add-failed");
    return ESP_ERR_ESPNOW_OWN_ADDRESS;
  }

  esp_now_peer_info_t peer_info = {};
  peer_info.channel = this->wifi_channel_;
  memcpy(peer_info.peer_addr, peer, ESP_NOW_ETH_ALEN);
  if (this->api_->add_peer(&peer_info) != ESP_OK) {
    ESP_LOGE(TAG, "Failed to add peer %s", format_mac_address_pretty(peer).c_str());
    this->status_momentary_warning("peer-add-failed");
    return ESP_FAIL;
  }

  bool found = false;
  for (auto &it : this->peers_) {
    if (memcmp(it.address, peer, ESP_NOW_ETH_ALEN) == 0) {
      found = true;
      break;
    }
  }
  if (!found) {
    ESPNowPeer new_peer;
    memcpy(new_peer.address, peer, ESP_NOW_ETH_ALEN);
    this->peers_.push_back(new_peer);
  }

  return ESP_OK;
}

espnow_err_t ESPNowComponent::del_peer(const uint8_t *peer) {
  if (this->state_ != ESPNOW_STATE_ENABLED || this->is_failed()) {
    return ESP_ERR_ESPNOW_NOT_INIT;
  }
  if (this->api_->del_peer(peer) != 0) {
    ESP_LOGE(TAG, "Failed to delete peer %s", format_mac_address_pretty(peer).c_str());
    this->status_momentary_warning("peer-del-failed");
    return ESP_FAIL;
  }
  for (auto it = this->peers_.begin(); it != this->peers_.end(); ++it) {
    if (memcmp(it->address, peer, ESP_NOW_ETH_ALEN) == 0) {
      this->peers_.erase(it);
      break;
    }
  }
  return ESP_OK;
}

}  // namespace esphome::espnow

#endif  // defined(USE_ESP32) || defined(USE_ESP8266)
