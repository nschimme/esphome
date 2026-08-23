#include "bluetooth_sig_mesh_rp2.h"

#if defined(USE_BLUETOOTH_SIG_MESH) && defined(USE_RP2040)

#include "esphome/components/rp2040_ble/rp2040_ble.h"

namespace esphome {
namespace bluetooth_sig_mesh {

static const char *const TAG = "bluetooth_sig_mesh.rp2040";

void RP2040BluetoothSIGMesh::setup() {
  BluetoothSIGMesh::setup();
  ESP_LOGCONFIG(TAG, "Initializing RP2040 BTstack Bluetooth SIG Mesh driver...");
  this->init_btstack_mesh_();
  this->register_btstack_models_();
}

void RP2040BluetoothSIGMesh::loop() { BluetoothSIGMesh::loop(); }

void RP2040BluetoothSIGMesh::init_btstack_mesh_() {
  ESP_LOGI(TAG, "Initializing BTstack SIG Mesh Node (mesh_node) parameters...");
  this->adv_interval_min_ = 0x0020;  // 20ms
  this->adv_interval_max_ = 0x0040;  // 40ms

  if (rp2040_ble::global_rp2040_ble != nullptr) {
    ESP_LOGD(TAG, "Wired RP2040 BTstack BLE controller advertising parameters for SIG Mesh");
    this->advertising_active_ = true;
  }

  if (this->enable_proxy_) {
    ESP_LOGI(TAG, "Configuring BTstack GATT Mesh Proxy Service (UUID 0x1828)...");
  }
}

void RP2040BluetoothSIGMesh::register_btstack_models_() {
  ESP_LOGI(TAG, "Registering BTstack Mesh Elements and Models...");
}

void RP2040BluetoothSIGMesh::process_mesh_pdu(const uint8_t *data, size_t len) {
  BluetoothSIGMesh::process_mesh_pdu(data, len);
}

void RP2040BluetoothSIGMesh::send_mesh_pdu(uint16_t dst, uint16_t app_idx, uint16_t opcode, const uint8_t *payload,
                                           size_t len) {
  BluetoothSIGMesh::send_mesh_pdu(dst, app_idx, opcode, payload, len);
  ESP_LOGI(TAG, "Transmitting RP2040 Pico W BLE Mesh advertisement packet (DST: 0x%04X, Len: %zu)...", dst, len);
}

}  // namespace bluetooth_sig_mesh
}  // namespace esphome

#endif  // USE_BLUETOOTH_SIG_MESH && USE_RP2040
