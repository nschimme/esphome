#pragma once

#ifdef USE_BLUETOOTH_SIG_MESH

#include "esphome/components/bluetooth_sig_mesh/bluetooth_sig_mesh_client.h"
#include "esphome/components/switch/switch.h"

namespace esphome {
namespace bluetooth_sig_mesh {

class BluetoothSIGMeshSwitch : public switch_::Switch, public BluetoothSIGMeshClientEntity {
 protected:
  void on_mesh_message_(uint16_t opcode, const uint8_t *payload, size_t len) override {
    if (opcode == OPCODE_GENERIC_ONOFF_STATUS && len >= 1) {
      bool state = (payload[0] & 0x01) != 0;
      this->publish_state(state);
    }
  }

  void write_state(bool state) override {
    if (this->parent_ != nullptr) {
      this->parent_->send_onoff(this->dst_address_, state, true);
    }
    this->publish_state(state);
  }
};

}  // namespace bluetooth_sig_mesh
}  // namespace esphome

#endif  // USE_BLUETOOTH_SIG_MESH
