#include "espnow_transport.h"
#include "esphome/core/log.h"

namespace esphome {
namespace espnow {

static const char *const TAG = "espnow.transport";

void ESPNowTransport::setup() {
  this->parent_->register_received_handler(this);
}

bool ESPNowTransport::on_received(const ESPNowRecvInfo &info, const uint8_t *data, uint8_t size) {
  ESP_LOGD(TAG, "Received packet from %s", format_mac_address_pretty(info.src_addr).c_str());
  this->process_(std::vector<uint8_t>(data, data + size));
  return true;
}

void ESPNowTransport::send_packet(const std::vector<uint8_t> &buf) const {
  if (!std::all_of(this->default_peer_.begin(), this->default_peer_.end(), [](uint8_t i) { return i == 0; })) {
    // A default peer is set, send to it.
    esp_err_t err = this->parent_->send(this->default_peer_.data(), buf);
    if (err != ESP_OK) {
      ESP_LOGW(TAG, "Failed to send packet to default peer: %s", esp_err_to_name(err));
    }
    return;
  }

  if (this->use_broadcast_) {
    // Broadcast is enabled, send to all devices on the same channel.
    esp_err_t err = this->parent_->send(ESPNOW_BROADCAST_ADDR, buf);
    if (err != ESP_OK) {
      ESP_LOGW(TAG, "Failed to send broadcast packet: %s", esp_err_to_name(err));
    }
    return;
  }

  const auto &peers = this->parent_->get_peers();
  if (!peers.empty()) {
    // No default peer or broadcast, send to all configured peers.
    for (const auto &peer : peers) {
      esp_err_t err = this->parent_->send(peer.address, buf);
      if (err != ESP_OK) {
        ESP_LOGW(TAG, "Failed to send packet to peer %s: %s", format_mac_address_pretty(peer.address).c_str(),
                 esp_err_to_name(err));
      }
    }
    return;
  }

  ESP_LOGE(TAG, "No peer set for ESPNow transport. Cannot send packet.");
}

size_t ESPNowTransport::get_max_packet_size() {
  // ESP-NOW has a maximum data length of 250 bytes.
  return ESP_NOW_MAX_DATA_LEN;
}

}  // namespace espnow
}  // namespace esphome
