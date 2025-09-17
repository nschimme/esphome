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
  if (std::all_of(this->default_peer_.begin(), this->default_peer_.end(), [](uint8_t i) { return i == 0; })) {
    ESP_LOGE(TAG, "No default peer set for ESPNow transport. Cannot send packet.");
    return;
  }
  auto *self = const_cast<ESPNowTransport *>(this);
  esp_err_t err = self->parent_->send(this->default_peer_.data(), buf);
  if (err != ESP_OK) {
    ESP_LOGW(TAG, "Failed to send packet: %s", esp_err_to_name(err));
  }
}

size_t ESPNowTransport::get_max_packet_size() {
  // ESP-NOW has a maximum data length of 250 bytes.
  return ESP_NOW_MAX_DATA_LEN;
}

}  // namespace espnow
}  // namespace esphome
