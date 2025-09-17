#pragma once

#include "esphome/components/ota/ota_backend.h"
#include "esphome/components/packet_transport/packet_transport.h"
#include "esphome/core/component.h"

namespace esphome {
namespace ota {

class PacketTransportProxyOTABackend : public OTABackend {
 public:
  static PacketTransportProxyOTABackend *get_instance();
  void set_transport(packet_transport::PacketTransport *transport) { this->transport_ = transport; }

  OTAResponseTypes begin(size_t image_size) override;
  void set_update_md5(const char *md5) override;
  OTAResponseTypes write(uint8_t *data, size_t len) override;
  OTAResponseTypes end() override;
  void abort() override;
  bool supports_compression() override { return false; }

 private:
  static PacketTransportProxyOTABackend *instance_;
  PacketTransportProxyOTABackend() = default;

  packet_transport::PacketTransport *transport_{nullptr};
  std::string md5_;
};

}  // namespace ota
}  // namespace esphome
