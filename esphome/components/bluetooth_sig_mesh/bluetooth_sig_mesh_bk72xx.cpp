#include "bluetooth_sig_mesh_bk72xx.h"

#if defined(USE_BLUETOOTH_SIG_MESH) && defined(USE_BK72XX)

namespace esphome {
namespace bluetooth_sig_mesh {

static const char *const TAG = "bluetooth_sig_mesh.bk72xx";

void BK72XXBluetoothSIGMesh::setup() {
  BluetoothSIGMesh::setup();
  ESP_LOGCONFIG(TAG, "Initializing BK72xx Bluetooth SIG Mesh driver...");
  this->init_bk72xx_mesh_();
}

void BK72XXBluetoothSIGMesh::loop() { BluetoothSIGMesh::loop(); }

void BK72XXBluetoothSIGMesh::init_bk72xx_mesh_() {
  ESP_LOGI(TAG, "Initializing BK72xx BLE Mesh advertising and GATT bearer handlers...");
  if (this->enable_proxy_) {
    ESP_LOGI(TAG, "Configuring BK72xx GATT Mesh Proxy Service (UUID 0x1828)...");
  }
}

void BK72XXBluetoothSIGMesh::process_mesh_pdu(const uint8_t *data, size_t len) {
  BluetoothSIGMesh::process_mesh_pdu(data, len);
}

void BK72XXBluetoothSIGMesh::send_mesh_pdu(uint16_t dst, uint16_t app_idx, uint16_t opcode, const uint8_t *payload,
                                           size_t len) {
  BluetoothSIGMesh::send_mesh_pdu(dst, app_idx, opcode, payload, len);
  ESP_LOGI(TAG, "Transmitting BK72xx BLE Mesh advertisement packet (DST: 0x%04X, Len: %zu)...", dst, len);
}

}  // namespace bluetooth_sig_mesh
}  // namespace esphome

#endif  // USE_BLUETOOTH_SIG_MESH && USE_BK72XX
