#pragma once

#if defined(USE_BLUETOOTH_SIG_MESH) && defined(USE_ESP32)

#include "bluetooth_sig_mesh.h"
#include <esp_gap_ble_api.h>

namespace esphome {
namespace bluetooth_sig_mesh {

class ESP32BluetoothSIGMesh : public BluetoothSIGMesh {
 public:
  ESP32BluetoothSIGMesh() = default;

  void setup() override;
  void loop() override;

  void process_mesh_pdu(const uint8_t *data, size_t len) override;
  void send_mesh_pdu(uint16_t dst, uint16_t app_idx, uint16_t opcode, const uint8_t *payload, size_t len) override;

 protected:
  void init_esp32_mesh_();
  void register_esp32_models_();
  void gap_event_handler_(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param);

  esp_ble_adv_params_t adv_params_{};
};

}  // namespace bluetooth_sig_mesh
}  // namespace esphome

#endif  // USE_BLUETOOTH_SIG_MESH && USE_ESP32
