#pragma once

#ifdef USE_BLUETOOTH_SIG_MESH

#include "esphome/components/bluetooth_sig_mesh/bluetooth_sig_mesh.h"
#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include <vector>

#if defined(USE_ESP32) && defined(USE_ESP32_BLE_SERVER)
#include "esphome/components/esp32_ble_server/ble_server.h"
#endif

namespace esphome {
namespace bluetooth_sig_mesh_proxy {

class BluetoothSIGMeshProxy : public Component {
 public:
  void set_mesh_parent(bluetooth_sig_mesh::BluetoothSIGMesh *parent) { this->parent_ = parent; }

  void setup() override;
  void notify_data_out(const uint8_t *data, size_t len);

 protected:
  bluetooth_sig_mesh::BluetoothSIGMesh *parent_{nullptr};
#if defined(USE_ESP32) && defined(USE_ESP32_BLE_SERVER)
  esp32_ble_server::BLECharacteristic *proxy_data_out_char_{nullptr};
#endif
};

}  // namespace bluetooth_sig_mesh_proxy
}  // namespace esphome

#endif  // USE_BLUETOOTH_SIG_MESH
