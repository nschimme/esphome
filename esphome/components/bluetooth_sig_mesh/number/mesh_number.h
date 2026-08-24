#pragma once

#ifdef USE_BLUETOOTH_SIG_MESH

#include "esphome/components/bluetooth_sig_mesh/bluetooth_sig_mesh_client.h"
#include "esphome/components/number/number.h"

namespace esphome {
namespace bluetooth_sig_mesh {

class BluetoothSIGMeshNumber : public number::Number, public BluetoothSIGMeshClientEntity {
 protected:
  void control(float value) override {
    this->publish_state(value);
    int16_t level = static_cast<int16_t>(value);
    if (this->parent_ != nullptr) {
      this->parent_->send_level(this->dst_address_, level, true);
    }
  }

  void on_mesh_message_(uint16_t opcode, const uint8_t *payload, size_t len) override {
    if (opcode == OPCODE_GENERIC_LEVEL_STATUS && len >= 2) {
      int16_t level =
          static_cast<int16_t>((static_cast<uint16_t>(payload[1]) << 8) | static_cast<uint16_t>(payload[0]));
      this->publish_state(static_cast<float>(level));
    }
  }
};

}  // namespace bluetooth_sig_mesh
}  // namespace esphome

#endif  // USE_BLUETOOTH_SIG_MESH
