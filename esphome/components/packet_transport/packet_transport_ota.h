#pragma once

#include "esphome/components/ota/ota_backend.h"
#include "packet_transport.h"

namespace esphome {
namespace packet_transport {

class PacketTransportOTABackend : public ota::OTABackend {
 public:
  void set_parent(PacketTransport *parent) { this->parent_ = parent; }

  ota::OTAResponseTypes begin(size_t image_size) override;
  void set_update_md5(const char *md5) override;
  ota::OTAResponseTypes write(uint8_t *data, size_t len) override;
  ota::OTAResponseTypes end() override;
  void abort() override;

 private:
  PacketTransport *parent_;
};

}  // namespace packet_transport
}  // namespace esphome
