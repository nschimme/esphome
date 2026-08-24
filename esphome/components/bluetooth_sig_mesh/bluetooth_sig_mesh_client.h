#pragma once

#ifdef USE_BLUETOOTH_SIG_MESH

#include "esphome/components/bluetooth_sig_mesh/bluetooth_sig_mesh.h"
#include "esphome/components/bluetooth_sig_mesh/bluetooth_sig_mesh_node.h"
#include "esphome/core/component.h"

namespace esphome {
namespace bluetooth_sig_mesh {

class BluetoothSIGMeshClientEntity : public Component {
 public:
  void set_parent(BluetoothSIGMesh *parent) { this->parent_ = parent; }
  void set_dst_address(uint16_t dst_address) { this->dst_address_ = dst_address; }
  void set_node(BluetoothSIGMeshNode *node) {
    if (node != nullptr) {
      this->parent_ = node->get_parent();
      this->dst_address_ = node->get_unicast_address();
    }
  }

  void setup() override {
    if (this->parent_ != nullptr) {
      this->parent_->add_node_seen_callback([this](uint16_t src, uint16_t opcode, const uint8_t *payload, size_t len) {
        if (src == this->dst_address_) {
          this->on_mesh_message_(opcode, payload, len);
        }
      });
    }
  }

 protected:
  virtual void on_mesh_message_(uint16_t opcode, const uint8_t *payload, size_t len) = 0;

  BluetoothSIGMesh *parent_{nullptr};
  uint16_t dst_address_{0x0001};
};

}  // namespace bluetooth_sig_mesh
}  // namespace esphome

#endif  // USE_BLUETOOTH_SIG_MESH
