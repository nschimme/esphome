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

  void process_mesh_pdu(const uint8_t *data, size_t len) override;
  void send_mesh_pdu(uint16_t dst, uint16_t app_idx, const uint8_t *payload, size_t len) override;

 protected:
  void init_btstack_mesh_();
  void register_btstack_models_();
};

}  // namespace bluetooth_sig_mesh
}  // namespace esphome

#endif  // USE_BLUETOOTH_SIG_MESH && USE_RP2040
