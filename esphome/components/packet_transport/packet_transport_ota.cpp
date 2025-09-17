#include "packet_transport_ota.h"
#include "esphome/core/log.h"
#include <Update.h>

namespace esphome {
namespace packet_transport {

static const char *const TAG = "packet_transport.ota";

ota::OTAResponseTypes PacketTransportOTABackend::begin(size_t image_size) {
  if (!Update.begin(image_size, U_FLASH)) {
    return ota::OTA_RESPONSE_ERROR_BEGIN;
  }
  return ota::OTA_RESPONSE_OK;
}

void PacketTransportOTABackend::set_update_md5(const char *md5) {
  Update.setMD5(md5);
}

ota::OTAResponseTypes PacketTransportOTABackend::write(uint8_t *data, size_t len) {
  if (Update.write(data, len) != len) {
    return ota::OTA_RESPONSE_ERROR_WRITE;
  }
  return ota::OTA_RESPONSE_OK;
}

ota::OTAResponseTypes PacketTransportOTABackend::end() {
  if (!Update.end()) {
    return ota::OTA_RESPONSE_ERROR_END;
  }
  return ota::OTA_RESPONSE_OK;
}

void PacketTransportOTABackend::abort() {
  Update.abort();
}

}  // namespace packet_transport
}  // namespace esphome
