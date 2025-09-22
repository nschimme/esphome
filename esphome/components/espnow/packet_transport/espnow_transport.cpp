#include "esphome/core/log.h"
#include "espnow_transport.h"

namespace esphome {
namespace espnow {

static const char *const TAG = "espnow_transport";

void ESPNowTransport::dump_config() {
  ESP_LOGCONFIG(TAG, "ESPNow Transport:");
  ESP_LOGCONFIG(TAG, "  Parent: %s", this->parent_->get_name().c_str());
}

bool ESPNowTransport::on_received(const ESPNowRecvInfo &info, const uint8_t *data, uint8_t size) {
  this->process_(std::vector<uint8_t>(data, data + size));
  return true;
}

void ESPNowTransport::send_packet(const std::vector<uint8_t> &buf) const {
  for (auto &peer : this->parent_->get_peers()) {
    this->parent_->send(peer.address, buf);
  }
}

}  // namespace espnow
}  // namespace esphome
