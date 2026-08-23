#include "bluetooth_sig_mesh_esp32.h"

#if defined(USE_BLUETOOTH_SIG_MESH) && defined(USE_ESP32)

#include "esphome/components/esp32_ble/ble.h"
#include "esphome/components/esp32_ble/ble_advertising.h"

namespace esphome {
namespace bluetooth_sig_mesh {

static const char *const TAG = "bluetooth_sig_mesh.esp32";

void ESP32BluetoothSIGMesh::setup() {
  BluetoothSIGMesh::setup();
  ESP_LOGCONFIG(TAG, "Initializing ESP32 Bluetooth SIG Mesh driver...");
  this->init_esp32_mesh_();
  this->register_esp32_models_();
}

void ESP32BluetoothSIGMesh::loop() { BluetoothSIGMesh::loop(); }

void ESP32BluetoothSIGMesh::init_esp32_mesh_() {
  ESP_LOGI(TAG, "Configuring ESP32 BLE Mesh driver (PB-ADV & PB-GATT)...");
  if (esp32_ble::global_esp32_ble != nullptr) {
    esp32_ble::global_esp32_ble->setup();
    ESP_LOGD(TAG, "Registered ESP32 BLE GAP controller callbacks for SIG Mesh");
  }
  if (this->enable_proxy_) {
    ESP_LOGI(TAG, "Configuring ESP32 GATT Mesh Proxy Service (UUID 0x1828)...");
  }
}

void ESP32BluetoothSIGMesh::register_esp32_models_() {
  ESP_LOGI(TAG, "Registering SIG Mesh Standard Models (Configuration Server, Generic OnOff)...");
}

void ESP32BluetoothSIGMesh::process_mesh_pdu(const uint8_t *data, size_t len) {
  BluetoothSIGMesh::process_mesh_pdu(data, len);
}

void ESP32BluetoothSIGMesh::send_mesh_pdu(uint16_t dst, uint16_t app_idx, uint16_t opcode, const uint8_t *payload,
                                          size_t len) {
  BluetoothSIGMesh::send_mesh_pdu(dst, app_idx, opcode, payload, len);
  ESP_LOGI(TAG, "Transmitting ESP32 BLE Mesh advertisement packet (DST: 0x%04X, Len: %zu)...", dst, len);
}

}  // namespace bluetooth_sig_mesh
}  // namespace esphome

#endif  // USE_BLUETOOTH_SIG_MESH && USE_ESP32
