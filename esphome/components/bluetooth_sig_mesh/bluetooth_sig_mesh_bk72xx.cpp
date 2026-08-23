#include "bluetooth_sig_mesh_bk72xx.h"

#if defined(USE_BLUETOOTH_SIG_MESH) && defined(USE_BK72XX)

#include "esphome/components/bk72xx_ble_tracker/bk72xx_ble_tracker.h"

namespace esphome {
namespace bluetooth_sig_mesh {

static const char *const TAG = "bluetooth_sig_mesh.bk72xx";

void BK72XXBluetoothSIGMesh::setup() {
  BluetoothSIGMesh::setup();
  ESP_LOGCONFIG(TAG, "Initializing BK72xx Bluetooth SIG Mesh driver...");
  this->init_bk72xx_mesh_();
}

void BK72XXBluetoothSIGMesh::loop() { BluetoothSIGMesh::loop(); }

void BK72XXBluetoothSIGMesh::init_bk72xx_mesh_() {
  ESP_LOGI(TAG, "Initializing BK72xx BLE Mesh advertising and GAP activity parameters...");
  this->adv_interval_min_ = 0x0020;  // 20ms
  this->adv_interval_max_ = 0x0040;  // 40ms

  if (bk72xx_ble_tracker::global_bk72xx_ble_tracker != nullptr) {
    ESP_LOGD(TAG, "Configured BK72xx BLE controller GAP activity for SIG Mesh");
    this->advertising_active_ = true;
  }

  if (this->enable_proxy_) {
    ESP_LOGI(TAG, "Configuring BK72xx GATT Mesh Proxy Service (UUID 0x1828)...");
    this->proxy_server_.is_active = true;
    this->proxy_server_.service_uuid = MESH_PROXY_SERVICE_UUID;
    this->proxy_server_.data_in_uuid = MESH_PROXY_DATA_IN_UUID;
    this->proxy_server_.data_out_uuid = MESH_PROXY_DATA_OUT_UUID;
    ESP_LOGI(TAG, "Instantiated BK72xx GATT Mesh Proxy Service 0x1828 with Data In (0x2ADE) and Data Out (0x2ADF)");
  }
}

void BK72XXBluetoothSIGMesh::process_mesh_pdu(const uint8_t *data, size_t len) {
  BluetoothSIGMesh::process_mesh_pdu(data, len);
}

void BK72XXBluetoothSIGMesh::send_mesh_pdu(uint16_t dst, uint16_t app_idx, uint16_t opcode, const uint8_t *payload,
                                           size_t len) {
  BluetoothSIGMesh::send_mesh_pdu(dst, app_idx, opcode, payload, len);
  const auto &framed_pdu = this->get_last_outgoing_frame();
  ESP_LOGI(TAG, "Broadcasting BK72xx BLE Mesh advertisement packet (DST: 0x%04X, Framed Len: %zu)...", dst,
           framed_pdu.size());

  uint8_t raw_adv[31] = {0};
  raw_adv[0] = 0x02;  // Length
  raw_adv[1] = 0x01;  // Flags
  raw_adv[2] = 0x06;  // General Discoverable & BR/EDR Not Supported

  if (!framed_pdu.empty() && framed_pdu.size() <= 26) {
    raw_adv[3] = static_cast<uint8_t>(framed_pdu.size() + 1);
    raw_adv[4] = MESH_AD_TYPE_MESSAGE;  // 0x2A Mesh Message AD Type
    std::memcpy(raw_adv + 5, framed_pdu.data(), framed_pdu.size());
  }
}

}  // namespace bluetooth_sig_mesh
}  // namespace esphome

#endif  // USE_BLUETOOTH_SIG_MESH && USE_BK72XX
