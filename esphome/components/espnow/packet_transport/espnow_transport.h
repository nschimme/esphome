#pragma once

#include "esphome/components/espnow/espnow_component.h"
#include "esphome/components/packet_transport/packet_transport.h"

namespace esphome {
namespace espnow {

class ESPNowTransport : public packet_transport::PacketTransport, public ESPNowReceivedPacketHandler {
 public:
  void setup() override;

  void set_parent(ESPNowComponent *parent) { this->parent_ = parent; }

  // ESPNowReceivedPacketHandler
  bool on_received(const ESPNowRecvInfo &info, const uint8_t *data, uint8_t size) override;

 protected:
  // PacketTransport
  void send_packet(const std::vector<uint8_t> &buf) const override;
  size_t get_max_packet_size() override;

  ESPNowComponent *parent_;
};

}  // namespace espnow
}  // namespace esphome
