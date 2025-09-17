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
  // Broadcast for now, until we have a way to specify the peer
  this->parent_->send(ESPNOW_BROADCAST_ADDR, buf);
}

size_t ESPNowTransport::get_max_packet_size() {
  return ESP_NOW_MAX_DATA_LEN;
}

}  // namespace espnow
}  // namespace esphome
