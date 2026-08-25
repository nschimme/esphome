#pragma once

#ifdef USE_BLUETOOTH_SIG_MESH

#include "esphome/components/bluetooth_sig_mesh/client.h"
#include "esphome/components/cover/cover.h"

namespace esphome {
namespace bluetooth_sig_mesh {

class BluetoothSIGMeshCover : public cover::Cover, public BluetoothSIGMeshClientEntity {
 public:
  cover::CoverTraits get_traits() override {
    auto traits = cover::CoverTraits();
    traits.set_supports_position(true);
    traits.set_supports_toggle(true);
    return traits;
  }

 protected:
  void control(const cover::CoverCall &call) override {
    if (call.get_position().has_value()) {
      float pos = *call.get_position();
      this->position = pos;
      int16_t level = static_cast<int16_t>((pos * 65535.0f) - 32768.0f);
      if (this->parent_ != nullptr) {
        this->parent_->send_level(this->dst_address_, level, true);
      }
      this->publish_state();
    }
    if (call.get_stop()) {
      if (this->parent_ != nullptr) {
        int16_t current_level = static_cast<int16_t>((this->position * 65535.0f) - 32768.0f);
        this->parent_->send_level(this->dst_address_, current_level, true);
      }
    }
  }

  void on_mesh_message_(uint16_t opcode, const uint8_t *payload, size_t len) override {
    if (opcode == OPCODE_GENERIC_LEVEL_STATUS && len >= 2) {
      int16_t level =
          static_cast<int16_t>((static_cast<uint16_t>(payload[1]) << 8) | static_cast<uint16_t>(payload[0]));
      float pos = (static_cast<float>(level) + 32768.0f) / 65535.0f;
      if (pos < 0.0f)
        pos = 0.0f;
      if (pos > 1.0f)
        pos = 1.0f;
      this->position = pos;
      this->publish_state();
    }
  }
};

}  // namespace bluetooth_sig_mesh
}  // namespace esphome

#endif  // USE_BLUETOOTH_SIG_MESH
