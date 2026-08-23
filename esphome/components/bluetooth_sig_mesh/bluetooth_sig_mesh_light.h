#pragma once

#ifdef USE_BLUETOOTH_SIG_MESH

#include "esphome/components/bluetooth_sig_mesh/bluetooth_sig_mesh.h"
#include "esphome/components/light/light_output.h"
#include "esphome/core/component.h"

namespace esphome {
namespace bluetooth_sig_mesh {

class BluetoothSIGMeshLight : public light::LightOutput, public Component {
 public:
  void set_parent(BluetoothSIGMesh *parent) { this->parent_ = parent; }
  void set_dst_address(uint16_t dst_address) { this->dst_address_ = dst_address; }

  light::LightTraits get_traits() override {
    auto traits = light::LightTraits();
    traits.set_supported_color_modes({light::ColorMode::BRIGHTNESS});
    return traits;
  }

  void setup() override {
    if (this->parent_ != nullptr) {
      this->parent_->add_node_seen_callback([this](uint16_t src, uint16_t opcode, const uint8_t *payload, size_t len) {
        if (src == this->dst_address_) {
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
      });
    }
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
  BluetoothSIGMesh *parent_{nullptr};
  uint16_t dst_address_{0x0001};
  light::LightState *state_{nullptr};
};

}  // namespace bluetooth_sig_mesh
}  // namespace esphome

#endif  // USE_BLUETOOTH_SIG_MESH
