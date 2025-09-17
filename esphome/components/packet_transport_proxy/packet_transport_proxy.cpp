#include "packet_transport_proxy.h"
#include "esphome/core/log.h"
#include <vector>

#ifdef USE_PACKET_TRANSPORT_PROXY_OTA
#include "esphome/components/ota/ota_backend.h"

namespace esphome {
namespace ota {

std::unique_ptr<OTABackend> make_ota_backend() {
  return std::unique_ptr<OTABackend>(PacketTransportProxyOTABackend::get_instance());
}

} // namespace ota
} // namespace esphome
#endif

namespace esphome {
namespace ota {

static const char *const TAG = "ota.packet_transport_proxy";

PacketTransportProxyOTABackend *PacketTransportProxyOTABackend::instance_ = nullptr;

PacketTransportProxyOTABackend *PacketTransportProxyOTABackend::get_instance() {
  if (instance_ == nullptr) {
    instance_ = new PacketTransportProxyOTABackend();
  }
  return instance_;
}

OTAResponseTypes PacketTransportProxyOTABackend::begin(size_t image_size) {
  ESP_LOGD(TAG, "begin(size=%zu)", image_size);

  if (this->transport_ == nullptr) {
    ESP_LOGE(TAG, "Transport not set!");
    return OTA_RESPONSE_ERROR_UNKNOWN;
  }

  std::vector<uint8_t> packet;
  packet.push_back(0x00); // START
  packet.push_back(image_size & 0xFF);
  packet.push_back((image_size >> 8) & 0xFF);
  packet.push_back((image_size >> 16) & 0xFF);
  packet.push_back((image_size >> 24) & 0xFF);
  packet.insert(packet.end(), this->md5_.begin(), this->md5_.end());

  this->transport_->send_ota_packet(packet);
  return OTA_RESPONSE_OK;
}

void PacketTransportProxyOTABackend::set_update_md5(const char *md5) {
  ESP_LOGD(TAG, "set_update_md5(md5=%s)", md5);
  this->md5_ = md5;
}

OTAResponseTypes PacketTransportProxyOTABackend::write(uint8_t *data, size_t len) {
  ESP_LOGD(TAG, "write(len=%zu)", len);

  if (this->transport_ == nullptr) {
    ESP_LOGE(TAG, "Transport not set!");
    return OTA_RESPONSE_ERROR_UNKNOWN;
  }

  std::vector<uint8_t> packet;
  packet.push_back(0x01); // DATA
  packet.insert(packet.end(), data, data + len);

  this->transport_->send_ota_packet(packet);
  return OTA_RESPONSE_OK;
}

OTAResponseTypes PacketTransportProxyOTABackend::end() {
  ESP_LOGD(TAG, "end()");

  if (this->transport_ == nullptr) {
    ESP_LOGE(TAG, "Transport not set!");
    return OTA_RESPONSE_ERROR_UNKNOWN;
  }

  std::vector<uint8_t> packet;
  packet.push_back(0x02); // END

  this->transport_->send_ota_packet(packet);
  return OTA_RESPONSE_OK;
}

void PacketTransportProxyOTABackend::abort() {
  ESP_LOGD(TAG, "abort()");

  if (this->transport_ == nullptr) {
    ESP_LOGE(TAG, "Transport not set!");
    return;
  }

  std::vector<uint8_t> packet;
  packet.push_back(0x03); // ABORT

  this->transport_->send_ota_packet(packet);
}

}  // namespace ota
}  // namespace esphome
