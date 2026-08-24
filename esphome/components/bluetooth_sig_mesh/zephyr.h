#pragma once

#if defined(USE_BLUETOOTH_SIG_MESH) && defined(USE_ZEPHYR)

#include "bluetooth_sig_mesh.h"
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/hci.h>

namespace esphome {
namespace bluetooth_sig_mesh {

class ZephyrBluetoothSIGMesh : public BluetoothSIGMesh {
 public:
  ZephyrBluetoothSIGMesh() = default;

  void setup() override;
  void loop() override;

  void transmit_last_outgoing_frame() override;

 protected:
  void init_zephyr_mesh_();
};

}  // namespace bluetooth_sig_mesh
}  // namespace esphome

#endif  // USE_BLUETOOTH_SIG_MESH && USE_ZEPHYR
