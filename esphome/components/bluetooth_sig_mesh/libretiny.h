#pragma once

#if defined(USE_BLUETOOTH_SIG_MESH) && defined(USE_LIBRETINY)

#include "bluetooth_sig_mesh.h"

namespace esphome {
namespace bluetooth_sig_mesh {

class LibreTinyBluetoothSIGMesh : public BluetoothSIGMesh {
 public:
  LibreTinyBluetoothSIGMesh() = default;

  void setup() override;
  void loop() override;

  void transmit_last_outgoing_frame() override;

 protected:
  void init_libretiny_mesh_();
};

}  // namespace bluetooth_sig_mesh
}  // namespace esphome

#endif  // USE_BLUETOOTH_SIG_MESH && USE_LIBRETINY
