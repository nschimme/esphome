#pragma once

#include "esphome/core/component.h"
#include "esphome/components/packet_transport/packet_transport.h"
#include "esphome/components/espnow/espnow_component.h"
#include <vector>

namespace esphome {
namespace espnow {

static const uint16_t MAX_PACKET_SIZE = 250;

class ESPNowTransport : public packet_transport::PacketTransport, public ESPNowReceivedPacketHandler, public Parented<ESPNowComponent> {
 public:
  void dump_config() override;
  bool on_received(const ESPNowRecvInfo &info, const uint8_t *data, uint8_t size) override;
  float get_setup_priority() const override { return setup_priority::AFTER_WIFI; }

 protected:
  void send_packet(const std::vector<uint8_t> &buf) const override;
  size_t get_max_packet_size() override { return MAX_PACKET_SIZE; }
};

}  // namespace espnow
}  // namespace esphome
