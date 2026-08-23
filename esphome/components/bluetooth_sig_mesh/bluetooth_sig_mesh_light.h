#pragma once

#ifdef USE_BLUETOOTH_SIG_MESH

#include "esphome/components/bluetooth_sig_mesh/bluetooth_sig_mesh_client.h"
#include "esphome/components/light/light_output.h"

namespace esphome {
namespace bluetooth_sig_mesh {

class BluetoothSIGMeshLight : public light::LightOutput, public BluetoothSIGMeshClientEntity {
 public:
  light::LightTraits get_traits() override {
    auto traits = light::LightTraits();
    traits.set_supported_color_modes({light::ColorMode::BRIGHTNESS});
    return traits;
  }

  void setup_state(light::LightState *state) override {
    this->state_ = state;
  }

  void write_state(light::LightState *state) override {
    this->state_ = state;
    if (this->parent_ == nullptr) {
      return;
    }
    auto values = state->remote_values;
    bool is_on = values.is_on();
    if (!is_on) {
      this->parent_->send_onoff(this->dst_address_, false, true);
    } else {
      float brightness = values.get_brightness();
      uint16_t lightness = static_cast<uint16_t>(brightness * 65535.0f);
      this->parent_->send_lightness(this->dst_address_, lightness, true);
    }
  }

 protected:
  void on_mesh_message_(uint16_t opcode, const uint8_t *payload, size_t len) override {
    if (opcode == OPCODE_LIGHT_LIGHTNESS_STATUS && len >= 2) {
      uint16_t lightness = static_cast<uint16_t>(payload[0]) | (static_cast<uint16_t>(payload[1]) << 8);
      float brightness = static_cast<float>(lightness) / 65535.0f;
      if (this->state_ != nullptr) {
        auto call = this->state_->make_call();
        call.set_brightness(brightness);
        call.set_state(brightness > 0.0f);
        call.perform();
      }
    }
  }

  light::LightState *state_{nullptr};
};

}  // namespace bluetooth_sig_mesh
}  // namespace esphome

#endif  // USE_BLUETOOTH_SIG_MESH
