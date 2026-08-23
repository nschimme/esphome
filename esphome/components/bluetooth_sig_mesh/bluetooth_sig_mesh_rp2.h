#pragma once

#if defined(USE_BLUETOOTH_SIG_MESH) && defined(USE_RP2040)

#include "bluetooth_sig_mesh.h"

namespace esphome {
namespace bluetooth_sig_mesh {

class RP2040BluetoothSIGMesh : public BluetoothSIGMesh {
 public:
  RP2040BluetoothSIGMesh() = default;

  void setup() override;
  void loop() override;

 protected:
  void init_btstack_mesh_();
};

}  // namespace bluetooth_sig_mesh
}  // namespace esphome

#endif  // USE_BLUETOOTH_SIG_MESH && USE_RP2040
