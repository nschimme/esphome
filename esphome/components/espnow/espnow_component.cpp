#include "espnow_component.h"
#include "espnow_hal.h"
#include "esphome/core/defines.h"
#include "esphome/core/log.h"
#include <cstring>
#include <memory>

#ifdef USE_WIFI
#include "esphome/components/wifi/wifi_component.h"
#endif

namespace esphome::espnow {

static constexpr const char *TAG = "espnow";

ESPNowComponent *global_esp_now = nullptr;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

std::string peer_str(uint8_t *peer) {
  if (peer == nullptr || peer[0] == 0) {
    return "[Not Set]";
  } else if (memcmp(peer, ESPNOW_BROADCAST_ADDR, ESPNOW_ETH_ALEN) == 0) {
    return "[Broadcast]";
  } else if (memcmp(peer, ESPNOW_MULTICAST_ADDR, ESPNOW_ETH_ALEN) == 0) {
    return "[Multicast]";
  } else {
    return format_mac_address_pretty(peer);
  }
}

ESPNowComponent::ESPNowComponent() { global_esp_now = this; }

void ESPNowComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "espnow:");
  if (this->is_disabled()) {
    ESP_LOGCONFIG(TAG, "  Disabled");
    return;
  }
  ESP_LOGCONFIG(TAG,
                "  Own address: %s\n"
                "  Wi-Fi channel: %d",
                format_mac_address_pretty(this->own_address_).c_str(), this->wifi_channel_);
#ifdef USE_WIFI
  ESP_LOGCONFIG(TAG, "  Wi-Fi enabled: %s", YESNO(this->is_wifi_enabled()));
#endif
}

void ESPNowComponent::on_data_received_callback(const ESPNowRecvInfo &info, const uint8_t *data, int size) {
  ESPNowPacket *packet = this->receive_packet_pool_.allocate();
  if (packet == nullptr) {
    this->receive_packet_queue_.increment_dropped_count();
    return;
  }
  packet->load_received_data(info, data, size);
  this->receive_packet_queue_.push(packet);
}

void ESPNowComponent::on_data_sent_callback(const uint8_t *mac_addr, bool success) {
  ESPNowPacket *packet = this->receive_packet_pool_.allocate();
  if (packet == nullptr) {
    this->receive_packet_queue_.increment_dropped_count();
    return;
  }
  packet->load_sent_data(mac_addr, success);
  this->receive_packet_queue_.push(packet);
}

bool ESPNowComponent::is_wifi_enabled() {
#ifdef USE_WIFI
  return wifi::global_wifi_component != nullptr && !wifi::global_wifi_component->is_disabled();
#else
  return false;
#endif
}

void ESPNowComponent::setup() {
  this->hal_ = get_esp_now_hal();
  this->hal_->set_callbacks(
      [this](const ESPNowRecvInfo &info, const uint8_t *data, int size) { this->on_data_received_callback(info, data, size); },
      [this](const uint8_t *mac_addr, bool success) { this->on_data_sent_callback(mac_addr, success); });

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
    this->apply_wifi_channel();
  }
  this->get_wifi_channel();

  if (!this->hal_->init()) {
    ESP_LOGE(TAG, "esp_now_init failed");
    this->mark_failed();
    return;
  }

  this->hal_->get_mac_address(this->own_address_);

#ifdef USE_DEEP_SLEEP
  this->hal_->set_wake_window(50);
#endif

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

  if (!this->hal_->deinit()) {
    ESP_LOGE(TAG, "esp_now_deinit failed!");
  }
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
  this->hal_->set_wifi_channel(this->wifi_channel_);
}

void ESPNowComponent::loop() {
#ifdef USE_WIFI
  if (wifi::global_wifi_component != nullptr && wifi::global_wifi_component->is_connected()) {
    int32_t new_channel = wifi::global_wifi_component->get_wifi_channel();
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
        const ESPNowRecvInfo &info = packet->get_receive_info();
        if (!this->hal_->is_peer_exist(info.src_addr)) {
          bool handled = false;
          for (auto *handler : this->unknown_peer_handlers_) {
            if (handler->on_unknown_peer(info, packet->packet_.receive.data, packet->packet_.receive.size)) {
              handled = true;
              break;
            }
          }
          if (!handled && this->auto_add_peer_) {
            this->add_peer(info.src_addr);
          }
        }
        if (this->hal_->is_peer_exist(info.src_addr)) {
#if ESPHOME_LOG_LEVEL >= ESPHOME_LOG_LEVEL_VERBOSE
          ESP_LOGV(TAG, "<<< [%s -> %s] %s", format_mac_address_pretty(info.src_addr).c_str(),
                   format_mac_address_pretty(info.des_addr).c_str(),
                   format_hex_pretty(packet->packet_.receive.data, packet->packet_.receive.size).c_str());
#endif
          if (memcmp(info.des_addr, ESPNOW_BROADCAST_ADDR, ESPNOW_ETH_ALEN) == 0) {
            for (auto *handler : this->broadcasted_handlers_) {
              if (handler->on_broadcasted(info, packet->packet_.receive.data, packet->packet_.receive.size))
                break;
            }
          } else {
            for (auto *handler : this->received_handlers_) {
              if (handler->on_received(info, packet->packet_.receive.data, packet->packet_.receive.size))
                break;
            }
          }
        }
        break;
      }
      case ESPNowPacket::SENT: {
#if ESPHOME_LOG_LEVEL >= ESPHOME_LOG_LEVEL_VERBOSE
        ESP_LOGV(TAG, ">>> [%s] %s", format_mac_address_pretty(packet->packet_.sent.address).c_str(),
                 packet->packet_.sent.success ? "OK" : "Failed");
#endif
        if (this->current_send_packet_ != nullptr) {
          this->current_send_packet_->callback_(packet->packet_.sent.success);
          this->send_packet_pool_.release(this->current_send_packet_);
          this->current_send_packet_ = nullptr;
        }
        break;
      }
      default:
        break;
    }
    this->receive_packet_pool_.release(packet);
    packet = this->receive_packet_queue_.pop();
  }

  if (this->current_send_packet_ == nullptr) {
    this->send_();
  }

  uint16_t received_dropped = this->receive_packet_queue_.get_and_reset_dropped_count();
  if (received_dropped > 0) {
    ESP_LOGW(TAG, "Dropped %u received packets due to buffer overflow", received_dropped);
  }

  uint16_t send_dropped = this->send_packet_queue_.get_and_reset_dropped_count();
  if (send_dropped > 0) {
    ESP_LOGW(TAG, "Dropped %u send packets due to buffer overflow", send_dropped);
  }
}

uint8_t ESPNowComponent::get_wifi_channel() {
  this->wifi_channel_ = this->hal_->get_wifi_channel();
  return this->wifi_channel_;
}

bool ESPNowComponent::send(const uint8_t *peer_address, const uint8_t *payload, size_t size,
                           const send_callback_t &callback) {
  if (this->state_ != ESPNOW_STATE_ENABLED) {
    return false;
  }
  if (this->is_failed()) {
    return false;
  }
  if (peer_address == nullptr) {
    return false;
  }
  if (memcmp(peer_address, this->own_address_, ESPNOW_ETH_ALEN) == 0) {
    return false;
  }
  if (size > ESPNOW_MAX_DATA_LEN) {
    return false;
  }
  if (!this->hal_->is_peer_exist(peer_address)) {
    if (memcmp(peer_address, ESPNOW_BROADCAST_ADDR, ESPNOW_ETH_ALEN) == 0 || this->auto_add_peer_) {
      if (!this->add_peer(peer_address)) {
        return false;
      }
    } else {
      return false;
    }
  }
  ESPNowSendPacket *packet = this->send_packet_pool_.allocate();
  if (packet == nullptr) {
    this->send_packet_queue_.increment_dropped_count();
    ESP_LOGE(TAG, "Failed to allocate send packet from pool");
    this->status_momentary_warning("send-packet-pool-full");
    return false;
  }
  packet->load_data(peer_address, payload, size, callback);
  this->send_packet_queue_.push(packet);
  return true;
}

void ESPNowComponent::send_() {
  ESPNowSendPacket *packet = this->send_packet_queue_.pop();
  if (packet == nullptr) {
    return;
  }
  this->current_send_packet_ = packet;
  if (!this->hal_->send(packet->address_, packet->data_, packet->size_)) {
    ESP_LOGE(TAG, "Failed to send packet to %s", format_mac_address_pretty(packet->address_).c_str());
    if (packet->callback_ != nullptr) {
      packet->callback_(false);
    }
    this->status_momentary_warning("send-failed");
    this->send_packet_pool_.release(packet);
    this->current_send_packet_ = nullptr;
  }
}

bool ESPNowComponent::add_peer(const uint8_t *peer) {
  if (this->state_ != ESPNOW_STATE_ENABLED || this->is_failed()) {
    return false;
  }
  if (memcmp(peer, this->own_address_, ESPNOW_ETH_ALEN) == 0) {
    this->status_momentary_warning("peer-add-failed");
    return false;
  }
  if (!this->hal_->is_peer_exist(peer)) {
    if (!this->hal_->add_peer(peer)) {
      ESP_LOGE(TAG, "Failed to add peer %s", format_mac_address_pretty(peer).c_str());
      this->status_momentary_warning("peer-add-failed");
      return false;
    }
  }
  for (auto &it : this->peers_) {
    if (it == peer) {
      return true;
    }
  }
  ESPNowPeer new_peer;
  memcpy(new_peer.address, peer, ESPNOW_ETH_ALEN);
  this->peers_.push_back(new_peer);
  return true;
}

bool ESPNowComponent::del_peer(const uint8_t *peer) {
  if (this->state_ != ESPNOW_STATE_ENABLED || this->is_failed()) {
    return false;
  }
  if (this->hal_->is_peer_exist(peer)) {
    if (!this->hal_->del_peer(peer)) {
      ESP_LOGE(TAG, "Failed to delete peer %s", format_mac_address_pretty(peer).c_str());
      this->status_momentary_warning("peer-del-failed");
      return false;
    }
  }
  for (auto it = this->peers_.begin(); it != this->peers_.end(); ++it) {
    if (*it == peer) {
      this->peers_.erase(it);
      break;
    }
  }
  return true;
}

}  // namespace esphome::espnow
