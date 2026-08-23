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

  void process_mesh_pdu(const uint8_t *data, size_t len) override;
  void send_mesh_pdu(uint16_t dst, uint16_t app_idx, uint16_t opcode, const uint8_t *payload, size_t len) override;

 protected:
  void init_bk72xx_mesh_();

  uint16_t adv_interval_min_{0x0020};
  uint16_t adv_interval_max_{0x0040};
  bool advertising_active_{false};
};

}  // namespace bluetooth_sig_mesh
}  // namespace esphome

#endif  // USE_BLUETOOTH_SIG_MESH && USE_BK72XX
