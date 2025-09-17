#pragma once

#include "esphome/components/ota/ota_backend.h"
#include "esphome/core/component.h"
#include "esphome/core/helpers.h"

#include <vector>

namespace esphome {
namespace packet_transport {

class PacketTransport;  // Forward declaration

class PacketTransportOTAComponent : public ota::OTAComponent, public Parented<PacketTransport> {
 public:
  void handle_ota_packet(const std::vector<uint8_t> &data);

 private:
  void on_ota_start_(const std::vector<uint8_t> &data);
  void on_ota_data_(const std::vector<uint8_t> &data);
  void on_ota_end_();
  void on_ota_abort_();

  std::unique_ptr<ota::OTABackend> ota_backend_;
  uint32_t ota_size_{0};
  uint32_t ota_written_{0};
};

}  // namespace packet_transport
}  // namespace esphome
