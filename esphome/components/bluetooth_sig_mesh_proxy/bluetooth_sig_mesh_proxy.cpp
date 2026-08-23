#include "bluetooth_sig_mesh_proxy.h"

#ifdef USE_BLUETOOTH_SIG_MESH

namespace esphome {
namespace bluetooth_sig_mesh_proxy {

static const char *const TAG = "bluetooth_sig_mesh_proxy";

void BluetoothSIGMeshProxy::setup() {
  ESP_LOGCONFIG(TAG, "Setting up Bluetooth SIG Mesh GATT Proxy Bridge...");
#if defined(USE_ESP32) && defined(USE_ESP32_BLE_SERVER)
  if (esp32_ble_server::global_ble_server != nullptr) {
    auto *proxy_service = esp32_ble_server::global_ble_server->create_service(
        esp32_ble::ESPBTUUID::from_uint16(bluetooth_sig_mesh::MESH_PROXY_SERVICE_UUID), true);
    if (proxy_service != nullptr) {
      auto *data_in_char = proxy_service->create_characteristic(
          bluetooth_sig_mesh::MESH_PROXY_DATA_IN_UUID, ESP_GATT_CHAR_PROP_BIT_WRITE | ESP_GATT_CHAR_PROP_BIT_WRITE_NR);
      if (data_in_char != nullptr) {
        data_in_char->on_write([this](const std::vector<uint8_t> &data) {
          if (this->parent_ != nullptr) {
            this->parent_->on_proxy_data_in_write(data.data(), data.size());
          }
        });
      }
      this->proxy_data_out_char_ = proxy_service->create_characteristic(
          bluetooth_sig_mesh::MESH_PROXY_DATA_OUT_UUID, ESP_GATT_CHAR_PROP_BIT_READ | ESP_GATT_CHAR_PROP_BIT_NOTIFY);
      proxy_service->start();
    }
  }
#endif
}

void BluetoothSIGMeshProxy::notify_data_out(const uint8_t *data, size_t len) {
#if defined(USE_ESP32) && defined(USE_ESP32_BLE_SERVER)
  if (this->proxy_data_out_char_ != nullptr && data != nullptr && len > 0) {
    this->proxy_data_out_char_->setValue(std::vector<uint8_t>(data, data + len));
    this->proxy_data_out_char_->notify();
  }
#endif
}

}  // namespace bluetooth_sig_mesh_proxy
}  // namespace esphome

#endif  // USE_BLUETOOTH_SIG_MESH
