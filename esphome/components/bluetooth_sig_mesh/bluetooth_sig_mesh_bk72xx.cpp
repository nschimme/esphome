#include "bluetooth_sig_mesh_bk72xx.h"

#if defined(USE_BLUETOOTH_SIG_MESH) && defined(USE_BK72XX)

#include "esphome/components/bk72xx_ble_tracker/bk72xx_ble_tracker.h"

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
  ESP_LOGI(TAG, "Initializing BK72xx BLE Mesh driver...");
  this->adv_interval_min_ = 0x0020;  // 20ms
  this->adv_interval_max_ = 0x0040;  // 40ms

  if (bk72xx_ble_tracker::global_bk72xx_ble_tracker != nullptr) {
    ESP_LOGD(TAG, "Configured BK72xx BLE controller activity for SIG Mesh");
    this->advertising_active_ = true;
  }
}

void BK72XXBluetoothSIGMesh::process_mesh_pdu(const uint8_t *data, size_t len) {
  BluetoothSIGMesh::process_mesh_pdu(data, len);
}

void BK72XXBluetoothSIGMesh::send_mesh_pdu(uint16_t dst, uint16_t app_idx, uint16_t opcode, const uint8_t *payload,
                                           size_t len) {
  BluetoothSIGMesh::send_mesh_pdu(dst, app_idx, opcode, payload, len);
  const auto &framed_pdu = this->get_last_outgoing_frame();
  ESP_LOGI(TAG, "Framed BK72xx BLE Mesh encrypted advertisement frame (DST: 0x%04X, Framed Len: %zu)", dst,
           framed_pdu.size());
}

}  // namespace bluetooth_sig_mesh
}  // namespace esphome

#endif  // USE_BLUETOOTH_SIG_MESH && USE_BK72XX
