#include "libretiny.h"

#if defined(USE_BLUETOOTH_SIG_MESH) && defined(USE_LIBRETINY)

#include "esphome/core/log.h"
#include <cstring>

namespace esphome {
namespace bluetooth_sig_mesh {

static const char *const TAG = "bluetooth_sig_mesh.libretiny";

void LibreTinyBluetoothSIGMesh::setup() {
  BluetoothSIGMesh::setup();
  ESP_LOGCONFIG(TAG, "Initializing LibreTiny Bluetooth SIG Mesh driver...");
  this->init_libretiny_mesh_();
}

void LibreTinyBluetoothSIGMesh::loop() { BluetoothSIGMesh::loop(); }

void LibreTinyBluetoothSIGMesh::init_libretiny_mesh_() {
  ESP_LOGI(TAG, "Configuring LibreTiny BLE Mesh GAP parameters...");
}

#if __has_include(<BLEDevice.h>)
#include <BLEDevice.h>
#include <BLEAdvertising.h>
#endif

void LibreTinyBluetoothSIGMesh::transmit_last_outgoing_frame() {
  const uint8_t *pdu_data = this->get_last_outgoing_frame_data();
  size_t pdu_len = this->get_last_outgoing_frame_len();
  if (pdu_len == 0 || pdu_data == nullptr || pdu_len > 26) {
    return;
  }

  uint8_t raw_adv[31] = {0};
  raw_adv[0] = 0x02;  // Length
  raw_adv[1] = 0x01;  // Flags
  raw_adv[2] = 0x06;  // General Discoverable & BR/EDR Not Supported
  raw_adv[3] = static_cast<uint8_t>(pdu_len + 1);
  raw_adv[4] = MESH_AD_TYPE_MESSAGE;  // 0x2A Mesh Message AD Type
  std::memcpy(raw_adv + 5, pdu_data, pdu_len);

  ESP_LOGD(TAG, "Transmitting LibreTiny Mesh GAP advertisement, len: %zu", pdu_len + 5);

#if __has_include(<BLEDevice.h>)
  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  if (pAdvertising != nullptr) {
    BLEAdvertisementData advertisementData;
    std::string payload(reinterpret_cast<const char *>(raw_adv), pdu_len + 5);
    advertisementData.addData(payload);
    pAdvertising->setAdvertisementData(advertisementData);
    pAdvertising->start();
  }
#endif
}

}  // namespace bluetooth_sig_mesh
}  // namespace esphome

#endif  // USE_BLUETOOTH_SIG_MESH && USE_LIBRETINY
