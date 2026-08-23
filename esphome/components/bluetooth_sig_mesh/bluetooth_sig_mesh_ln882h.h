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

  void process_mesh_pdu(const uint8_t *data, size_t len) override;
  void send_mesh_pdu(uint16_t dst, uint16_t app_idx, uint16_t opcode, const uint8_t *payload, size_t len) override;

 protected:
  void init_ln882h_mesh_();
};

}  // namespace bluetooth_sig_mesh
}  // namespace esphome

#endif  // USE_BLUETOOTH_SIG_MESH && USE_LN882H
