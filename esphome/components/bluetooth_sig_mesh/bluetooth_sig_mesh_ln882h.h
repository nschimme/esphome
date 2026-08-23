#pragma once

#if defined(USE_BLUETOOTH_SIG_MESH) && defined(USE_LN882H)

#include "bluetooth_sig_mesh.h"

namespace esphome {
namespace bluetooth_sig_mesh {

class LN882HBluetoothSIGMesh : public BluetoothSIGMesh {
 public:
  LN882HBluetoothSIGMesh() = default;

  void setup() override;
  void loop() override;

 protected:
  void init_ln882h_mesh_();
};

}  // namespace bluetooth_sig_mesh
}  // namespace esphome

#endif  // USE_BLUETOOTH_SIG_MESH && USE_LN882H
