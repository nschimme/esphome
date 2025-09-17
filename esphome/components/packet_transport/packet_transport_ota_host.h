#pragma once

#include "esphome/components/ota/ota_backend.h"
#include "packet_transport.h"

namespace esphome {
namespace packet_transport {

class PacketTransportHostOTABackend : public ota::OTABackend {
 public:
  void set_parent(PacketTransport *parent) { this->parent_ = parent; }
  void set_provider(const char *provider) { this->provider_ = provider; }

  ota::OTAResponseTypes begin(size_t image_size) override;
  void set_update_md5(const char *md5) override;
  ota::OTAResponseTypes write(uint8_t *data, size_t len) override;
  ota::OTAResponseTypes end() override;
  void abort() override;

 private:
  PacketTransport *parent_;
  const char *provider_;
  const char *md5_;
};

}  // namespace packet_transport
}  // namespace esphome
