#include "bluetooth_sig_mesh.h"

#ifdef USE_BLUETOOTH_SIG_MESH

namespace esphome {
namespace bluetooth_sig_mesh {

static const char *const TAG = "bluetooth_sig_mesh";

BluetoothSIGMesh *global_bluetooth_sig_mesh = nullptr;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

void BluetoothSIGMesh::setup() {
  global_bluetooth_sig_mesh = this;
  ESP_LOGCONFIG(TAG, "Setting up Bluetooth SIG Mesh...");
}

void BluetoothSIGMesh::loop() {
  // Main loop processing for mesh PDU routing and proxy state updates
}

void BluetoothSIGMesh::dump_config() {
  ESP_LOGCONFIG(TAG, "Bluetooth SIG Mesh:");
  ESP_LOGCONFIG(TAG, "  Node enabled: %s", YESNO(this->enable_node_));
  ESP_LOGCONFIG(TAG, "  Proxy enabled: %s", YESNO(this->enable_proxy_));
  ESP_LOGCONFIG(TAG, "  Unicast address: 0x%04X", this->unicast_address_);
}

}  // namespace bluetooth_sig_mesh
}  // namespace esphome

#endif  // USE_BLUETOOTH_SIG_MESH
