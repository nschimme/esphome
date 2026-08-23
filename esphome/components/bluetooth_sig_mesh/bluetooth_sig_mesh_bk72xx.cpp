#include "bluetooth_sig_mesh_bk72xx.h"

#if defined(USE_BLUETOOTH_SIG_MESH) && defined(USE_BK72XX)

namespace esphome {
namespace bluetooth_sig_mesh {

static const char *const TAG = "bluetooth_sig_mesh.bk72xx";

void BK72XXBluetoothSIGMesh::setup() {
  BluetoothSIGMesh::setup();
  ESP_LOGCONFIG(TAG, "Initializing BK72xx Bluetooth SIG Mesh stack...");
  this->init_bk72xx_mesh_();
}

void BK72XXBluetoothSIGMesh::loop() { BluetoothSIGMesh::loop(); }

void BK72XXBluetoothSIGMesh::init_bk72xx_mesh_() {
  // BK72xx Bluetooth SIG Mesh initialization logic
}

}  // namespace bluetooth_sig_mesh
}  // namespace esphome

#endif  // USE_BLUETOOTH_SIG_MESH && USE_BK72XX
