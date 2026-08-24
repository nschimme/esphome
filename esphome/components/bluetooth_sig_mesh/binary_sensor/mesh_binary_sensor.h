#pragma once

#ifdef USE_BLUETOOTH_SIG_MESH

#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/bluetooth_sig_mesh/client.h"

namespace esphome {
namespace bluetooth_sig_mesh {

class BluetoothSIGMeshBinarySensor : public binary_sensor::BinarySensor, public BluetoothSIGMeshClientEntity {
 protected:
  void on_mesh_message_(uint16_t opcode, const uint8_t *payload, size_t len) override {
    if (opcode == OPCODE_GENERIC_ONOFF_STATUS && len >= 1) {
      bool state = (payload[0] & 0x01) != 0;
      this->publish_state(state);
    } else if (opcode == OPCODE_SENSOR_STATUS && len >= 3) {
      bool state = (payload[2] & 0x01) != 0;
      this->publish_state(state);
    }
  }
};

}  // namespace bluetooth_sig_mesh
}  // namespace esphome

#endif  // USE_BLUETOOTH_SIG_MESH
