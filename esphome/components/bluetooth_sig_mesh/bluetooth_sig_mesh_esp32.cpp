#include "bluetooth_sig_mesh_esp32.h"

#if defined(USE_BLUETOOTH_SIG_MESH) && defined(USE_ESP32)

namespace esphome {
namespace bluetooth_sig_mesh {

static const char *const TAG = "bluetooth_sig_mesh.esp32";

void ESP32BluetoothSIGMesh::setup() {
  BluetoothSIGMesh::setup();
  ESP_LOGCONFIG(TAG, "Initializing ESP32 Bluetooth SIG Mesh stack...");
  this->init_esp32_mesh_();
}

void ESP32BluetoothSIGMesh::loop() {
  BluetoothSIGMesh::loop();
}

void ESP32BluetoothSIGMesh::init_esp32_mesh_() {
  // ESP32 BLE Mesh stack initialization logic
}

}  // namespace bluetooth_sig_mesh
}  // namespace esphome

#endif  // USE_BLUETOOTH_SIG_MESH && USE_ESP32
