#pragma once

#ifdef USE_BLUETOOTH_SIG_MESH

#include "esphome/components/bluetooth_sig_mesh/client.h"
#include "esphome/components/sensor/sensor.h"

namespace esphome {
namespace bluetooth_sig_mesh {

class BluetoothSIGMeshSensor : public sensor::Sensor, public BluetoothSIGMeshClientEntity {
 public:
  BluetoothSIGMeshSensor() = default;

 protected:
  void on_mesh_message_(uint16_t opcode, const uint8_t *payload, size_t len) override {
    if (opcode == OPCODE_SENSOR_STATUS && len >= 4) {
      uint16_t prop_id = decode_uint16_le(payload);
      int16_t raw_val = decode_int16_le(payload + 2);
      float val = static_cast<float>(raw_val) / 100.0f;
      this->publish_state(val);
    }
  }
};

}  // namespace bluetooth_sig_mesh
}  // namespace esphome

#endif  // USE_BLUETOOTH_SIG_MESH
