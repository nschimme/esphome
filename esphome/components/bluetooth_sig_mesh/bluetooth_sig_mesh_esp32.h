#pragma once

#if defined(USE_BLUETOOTH_SIG_MESH) && defined(USE_ESP32)

#include "bluetooth_sig_mesh.h"

namespace esphome {
namespace bluetooth_sig_mesh {

class ESP32BluetoothSIGMesh : public BluetoothSIGMesh {
 public:
  ESP32BluetoothSIGMesh() = default;

  void setup() override;
  void loop() override;

 protected:
  void init_esp32_mesh_();
};

}  // namespace bluetooth_sig_mesh
}  // namespace esphome

#endif  // USE_BLUETOOTH_SIG_MESH && USE_ESP32
