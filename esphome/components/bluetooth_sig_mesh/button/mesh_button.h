#pragma once

#ifdef USE_BLUETOOTH_SIG_MESH

#include "esphome/components/bluetooth_sig_mesh/client.h"
#include "esphome/components/button/button.h"

namespace esphome {
namespace bluetooth_sig_mesh {

class BluetoothSIGMeshButton : public button::Button, public BluetoothSIGMeshClientEntity {
 protected:
  void press_action() override {
    if (this->parent_ != nullptr) {
      this->parent_->send_onoff(this->dst_address_, true, false);
    }
  }

  void on_mesh_message_(uint16_t opcode, const uint8_t *payload, size_t len) override {}
};

}  // namespace bluetooth_sig_mesh
}  // namespace esphome

#endif  // USE_BLUETOOTH_SIG_MESH
