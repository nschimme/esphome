#pragma once

#ifdef USE_BLUETOOTH_SIG_MESH

#include "esphome/components/bluetooth_sig_mesh/bluetooth_sig_mesh.h"
#include "esphome/core/automation.h"

namespace esphome {
namespace bluetooth_sig_mesh {

template<typename... Ts>
class SendOnOffAction : public Action<Ts...> {
 public:
  explicit SendOnOffAction(BluetoothSIGMesh *parent) : parent_(parent) {}
  TEMPLATABLE_VALUE(uint16_t, dst_address)
  TEMPLATABLE_VALUE(bool, state)

  void play(Ts... x) override {
    if (this->parent_ != nullptr) {
      uint16_t dst = this->dst_address_.value(x...);
      bool state = this->state_.value(x...);
      this->parent_->send_onoff(dst, state, true);
    }
  }

 protected:
  BluetoothSIGMesh *parent_;
};

template<typename... Ts>
class SendLevelAction : public Action<Ts...> {
 public:
  explicit SendLevelAction(BluetoothSIGMesh *parent) : parent_(parent) {}
  TEMPLATABLE_VALUE(uint16_t, dst_address)
  TEMPLATABLE_VALUE(int16_t, level)

  void play(Ts... x) override {
    if (this->parent_ != nullptr) {
      uint16_t dst = this->dst_address_.value(x...);
      int16_t level = this->level_.value(x...);
      this->parent_->send_level(dst, level, true);
    }
  }

 protected:
  BluetoothSIGMesh *parent_;
};

template<typename... Ts>
class SendLightnessAction : public Action<Ts...> {
 public:
  explicit SendLightnessAction(BluetoothSIGMesh *parent) : parent_(parent) {}
  TEMPLATABLE_VALUE(uint16_t, dst_address)
  TEMPLATABLE_VALUE(uint16_t, lightness)

  void play(Ts... x) override {
    if (this->parent_ != nullptr) {
      uint16_t dst = this->dst_address_.value(x...);
      uint16_t lightness = this->lightness_.value(x...);
      this->parent_->send_lightness(dst, lightness, true);
    }
  }

 protected:
  BluetoothSIGMesh *parent_;
};

}  // namespace bluetooth_sig_mesh
}  // namespace esphome

#endif  // USE_BLUETOOTH_SIG_MESH
