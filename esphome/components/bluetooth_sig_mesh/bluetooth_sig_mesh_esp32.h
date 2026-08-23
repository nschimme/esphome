#pragma once

#if defined(USE_BLUETOOTH_SIG_MESH) && defined(USE_ESP32)

#include "bluetooth_sig_mesh.h"

namespace esphome {
namespace bluetooth_sig_mesh {

class ESP32BluetoothSIGMesh : public BluetoothSIGMesh {
 public:
  ESP32BluetoothSIGMesh() = default;

  void setup() override;
  void loop() override;

  void process_mesh_pdu(const uint8_t *data, size_t len) override;
  void send_mesh_pdu(uint16_t dst, uint16_t app_idx, const uint8_t *payload, size_t len) override;

 protected:
  void init_esp32_mesh_();
  void register_esp32_models_();
};

}  // namespace bluetooth_sig_mesh
}  // namespace esphome

#endif  // USE_BLUETOOTH_SIG_MESH && USE_ESP32
