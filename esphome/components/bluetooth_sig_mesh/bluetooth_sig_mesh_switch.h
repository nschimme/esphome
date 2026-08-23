#pragma once

#ifdef USE_BLUETOOTH_SIG_MESH

#include "esphome/components/bluetooth_sig_mesh/bluetooth_sig_mesh.h"
#include "esphome/components/switch/switch.h"
#include "esphome/core/component.h"

namespace esphome {
namespace bluetooth_sig_mesh {

class BluetoothSIGMeshSwitch : public switch_::Switch, public Component {
 public:
  void set_parent(BluetoothSIGMesh *parent) { this->parent_ = parent; }
  void set_dst_address(uint16_t dst_address) { this->dst_address_ = dst_address; }

  void setup() override {
    if (this->parent_ != nullptr) {
      this->parent_->add_node_seen_callback([this](uint16_t src, uint16_t opcode, const uint8_t *payload, size_t len) {
        if (src == this->dst_address_) {
          if (opcode == OPCODE_GENERIC_ONOFF_STATUS && len >= 1) {
            bool state = (payload[0] & 0x01) != 0;
            this->publish_state(state);
          }
        }
      });
    }
  }

 protected:
  void write_state(bool state) override {
    if (this->parent_ != nullptr) {
      this->parent_->send_onoff(this->dst_address_, state, true);
    }
    this->publish_state(state);
  }

  BluetoothSIGMesh *parent_{nullptr};
  uint16_t dst_address_{0x0001};
};

}  // namespace bluetooth_sig_mesh
}  // namespace esphome

#endif  // USE_BLUETOOTH_SIG_MESH
