#include "packet_transport_ota_host.h"
#include "esphome/core/log.h"

namespace esphome {
namespace packet_transport {

static const char *const TAG = "packet_transport.ota_host";

ota::OTAResponseTypes PacketTransportHostOTABackend::begin(size_t image_size) {
  this->parent_->send_ota_begin(this->provider_, image_size, this->md5_);
  return ota::OTA_RESPONSE_OK;
}

void PacketTransportHostOTABackend::set_update_md5(const char *md5) {
  this->md5_ = md5;
}

ota::OTAResponseTypes PacketTransportHostOTABackend::write(uint8_t *data, size_t len) {
  this->parent_->send_ota_data(this->provider_, std::vector<uint8_t>(data, data + len));
  return ota::OTA_RESPONSE_OK;
}

ota::OTAResponseTypes PacketTransportHostOTABackend::end() {
  this->parent_->send_ota_end(this->provider_);
  return ota::OTA_RESPONSE_OK;
}

void PacketTransportHostOTABackend::abort() {
  // No-op
}

}  // namespace packet_transport
}  // namespace esphome
