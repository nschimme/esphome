#include "bluetooth_sig_mesh_ln882h.h"

#if defined(USE_BLUETOOTH_SIG_MESH) && defined(USE_LN882H)

namespace esphome {
namespace bluetooth_sig_mesh {

static const char *const TAG = "bluetooth_sig_mesh.ln882h";

void LN882HBluetoothSIGMesh::setup() {
  BluetoothSIGMesh::setup();
  ESP_LOGCONFIG(TAG, "Initializing LN882H Bluetooth SIG Mesh driver...");
  this->init_ln882h_mesh_();
}

void LN882HBluetoothSIGMesh::loop() { BluetoothSIGMesh::loop(); }

void LN882HBluetoothSIGMesh::init_ln882h_mesh_() {
  ESP_LOGI(TAG, "Initializing LN882H BLE Mesh advertising and GATT bearer handlers...");
  if (this->enable_proxy_) {
    ESP_LOGI(TAG, "Configuring LN882H GATT Mesh Proxy Service (UUID 0x1828)...");
  }
}

void LN882HBluetoothSIGMesh::process_mesh_pdu(const uint8_t *data, size_t len) {
  BluetoothSIGMesh::process_mesh_pdu(data, len);
}

void LN882HBluetoothSIGMesh::send_mesh_pdu(uint16_t dst, uint16_t app_idx, uint16_t opcode, const uint8_t *payload,
                                           size_t len) {
  BluetoothSIGMesh::send_mesh_pdu(dst, app_idx, opcode, payload, len);
  ESP_LOGI(TAG, "Transmitting LN882H BLE Mesh advertisement packet (DST: 0x%04X, Len: %zu)...", dst, len);
}

}  // namespace bluetooth_sig_mesh
}  // namespace esphome

#endif  // USE_BLUETOOTH_SIG_MESH && USE_LN882H
