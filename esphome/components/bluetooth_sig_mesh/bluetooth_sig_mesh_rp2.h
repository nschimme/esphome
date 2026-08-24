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
  void send_mesh_pdu(uint16_t dst, uint16_t app_idx, uint16_t opcode, const uint8_t *payload, size_t len) override;
  void transmit_last_outgoing_frame() override;

 protected:
  void init_btstack_mesh_();
  void register_btstack_models_();

  uint16_t adv_interval_min_{0x0020};
  uint16_t adv_interval_max_{0x0040};
  bool advertising_active_{false};

  std::array<uint8_t, 31> raw_adv_buffer_{0};
};

}  // namespace bluetooth_sig_mesh
}  // namespace esphome

#endif  // USE_BLUETOOTH_SIG_MESH && USE_RP2040
