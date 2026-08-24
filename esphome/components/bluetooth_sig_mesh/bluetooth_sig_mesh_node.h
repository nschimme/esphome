#pragma once

#ifdef USE_BLUETOOTH_SIG_MESH

#include "esphome/components/bluetooth_sig_mesh/bluetooth_sig_mesh.h"
#include "esphome/core/component.h"
#include <string>

namespace esphome {
namespace bluetooth_sig_mesh {

class BluetoothSIGMeshNode : public Component {
 public:
  void set_parent(BluetoothSIGMesh *parent) { this->parent_ = parent; }
  void set_unicast_address(uint16_t address) { this->unicast_address_ = address; }
  void set_device_key(const std::string &device_key_hex) { this->device_key_hex_ = device_key_hex; }

  void setup() override {
    if (this->parent_ != nullptr) {
      this->parent_->add_remote_node(this->unicast_address_, this->device_key_hex_);
    }
  }

  BluetoothSIGMesh *get_parent() const { return this->parent_; }
  uint16_t get_unicast_address() const { return this->unicast_address_; }

 protected:
  BluetoothSIGMesh *parent_{nullptr};
  uint16_t unicast_address_{0x0001};
  std::string device_key_hex_{};
};

}  // namespace bluetooth_sig_mesh
}  // namespace esphome

#endif  // USE_BLUETOOTH_SIG_MESH
