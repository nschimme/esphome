#pragma once

#ifdef USE_BLUETOOTH_SIG_MESH

#include "esphome/components/bluetooth_sig_mesh/client.h"
#include "esphome/components/light/light_output.h"

namespace esphome {
namespace bluetooth_sig_mesh {

class BluetoothSIGMeshLight : public light::LightOutput, public BluetoothSIGMeshClientEntity {
 public:
  void set_color_temperature(bool supported) { this->supports_cct_ = supported; }

  light::LightTraits get_traits() override {
    auto traits = light::LightTraits();
    if (this->supports_cct_) {
      traits.set_supported_color_modes({light::ColorMode::COLOR_TEMPERATURE});
      traits.set_min_mireds(153);  // 6500K
      traits.set_max_mireds(500);  // 2000K
    } else {
      traits.set_supported_color_modes({light::ColorMode::BRIGHTNESS});
    }
    return traits;
  }

  void setup_state(light::LightState *state) override { this->state_ = state; }

  void write_state(light::LightState *state) override {
    this->state_ = state;
    if (this->ignore_next_write_) {
      this->ignore_next_write_ = false;
      return;
    }
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
      if (this->supports_cct_ && values.get_color_mode() == light::ColorMode::COLOR_TEMPERATURE) {
        float mireds = values.get_color_temperature();
        uint16_t temp_kelvin = static_cast<uint16_t>(1000000.0f / mireds);
        this->parent_->send_ctl(this->dst_address_, lightness, temp_kelvin, 0, true);
      } else {
        this->parent_->send_lightness(this->dst_address_, lightness, true);
      }
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
        this->ignore_next_write_ = true;
        call.perform();
      }
    } else if (opcode == OPCODE_LIGHT_CTL_STATUS && len >= 4) {
      uint16_t lightness = static_cast<uint16_t>(payload[0]) | (static_cast<uint16_t>(payload[1]) << 8);
      uint16_t temp_kelvin = static_cast<uint16_t>(payload[2]) | (static_cast<uint16_t>(payload[3]) << 8);
      float brightness = static_cast<float>(lightness) / 65535.0f;
      if (this->state_ != nullptr) {
        auto call = this->state_->make_call();
        call.set_brightness(brightness);
        if (temp_kelvin >= 800 && temp_kelvin <= 20000) {
          float mireds = 1000000.0f / static_cast<float>(temp_kelvin);
          call.set_color_temperature(mireds);
        }
        call.set_state(brightness > 0.0f);
        this->ignore_next_write_ = true;
        call.perform();
      }
    }
  }

  bool ignore_next_write_{false};
  bool supports_cct_{false};
  light::LightState *state_{nullptr};
};

}  // namespace bluetooth_sig_mesh
}  // namespace esphome

#endif  // USE_BLUETOOTH_SIG_MESH
