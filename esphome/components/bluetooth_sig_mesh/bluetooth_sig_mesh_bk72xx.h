#pragma once

#if defined(USE_BLUETOOTH_SIG_MESH) && defined(USE_BK72XX)

#include "bluetooth_sig_mesh.h"

namespace esphome {
namespace bluetooth_sig_mesh {

class BK72XXBluetoothSIGMesh : public BluetoothSIGMesh {
 public:
  BK72XXBluetoothSIGMesh() = default;

  void setup() override;
  void loop() override;

 protected:
  void init_bk72xx_mesh_();
};

}  // namespace bluetooth_sig_mesh
}  // namespace esphome

#endif  // USE_BLUETOOTH_SIG_MESH && USE_BK72XX
