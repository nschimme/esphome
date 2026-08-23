#include "bluetooth_sig_mesh_rp2.h"

#if defined(USE_BLUETOOTH_SIG_MESH) && defined(USE_RP2040)

namespace esphome {
namespace bluetooth_sig_mesh {

static const char *const TAG = "bluetooth_sig_mesh.rp2040";

void RP2040BluetoothSIGMesh::setup() {
  BluetoothSIGMesh::setup();
  ESP_LOGCONFIG(TAG, "Initializing RP2040 BTstack Bluetooth SIG Mesh stack...");
  this->init_btstack_mesh_();
}

void RP2040BluetoothSIGMesh::loop() { BluetoothSIGMesh::loop(); }

void RP2040BluetoothSIGMesh::init_btstack_mesh_() {
  // BTstack SIG Mesh initialization logic for RP2040 Pico W
}

}  // namespace bluetooth_sig_mesh
}  // namespace esphome

#endif  // USE_BLUETOOTH_SIG_MESH && USE_RP2040
