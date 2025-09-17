#pragma once

#include "esphome/core/component.h"
#include "esphome/components/espnow/espnow_component.h"
#include "esphome/components/packet_transport/packet_transport.h"

namespace esphome {
namespace espnow {

/// ESPNow packet transport component.
class ESPNowTransport : public packet_transport::PacketTransport,
                        public ESPNowReceivedPacketHandler,
                        public Parented<ESPNowComponent> {
 public:
  void setup() override;

  /// Get the setup priority for this component.
  float get_setup_priority() const override { return setup_priority::AFTER_WIFI; }

  void set_default_peer(const std::array<uint8_t, 6> &default_peer) { this->default_peer_ = default_peer; }

  // =================================================================
  // ESPNowReceivedPacketHandler virtual methods
  // =================================================================

  /// Called when an ESPNow packet is received.
  bool on_received(const ESPNowRecvInfo &info, const uint8_t *data, uint8_t size) override;

 protected:
  // =================================================================
  // PacketTransport virtual methods
  // =================================================================

  /// Send a packet over the ESPNow transport.
  void send_packet(const std::vector<uint8_t> &buf) const override;

  /// Check if the transport is ready to send a packet.
  bool should_send() override { return true; }

  /// Get the maximum size of a packet that can be sent.
  size_t get_max_packet_size() override;

  std::array<uint8_t, 6> default_peer_{};
};

}  // namespace espnow
}  // namespace esphome
