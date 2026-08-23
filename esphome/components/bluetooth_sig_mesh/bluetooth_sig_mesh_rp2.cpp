#include "bluetooth_sig_mesh_rp2.h"

#if defined(USE_BLUETOOTH_SIG_MESH) && defined(USE_RP2040)

#include "esphome/components/rp2040_ble/rp2040_ble.h"

namespace esphome {
namespace bluetooth_sig_mesh {

static const char *const TAG = "bluetooth_sig_mesh.rp2040";

void RP2040BluetoothSIGMesh::setup() {
  BluetoothSIGMesh::setup();
  ESP_LOGCONFIG(TAG, "Initializing RP2040 BTstack Bluetooth SIG Mesh driver...");
  this->init_btstack_mesh_();
  this->register_btstack_models_();
}

void RP2040BluetoothSIGMesh::loop() { BluetoothSIGMesh::loop(); }

void RP2040BluetoothSIGMesh::init_btstack_mesh_() {
  ESP_LOGI(TAG, "Initializing BTstack SIG Mesh Node (mesh_node) parameters...");
  this->adv_interval_min_ = 0x0020;  // 20ms
  this->adv_interval_max_ = 0x0040;  // 40ms

  if (rp2040_ble::global_rp2040_ble != nullptr) {
    ESP_LOGD(TAG, "Wired RP2040 BTstack BLE controller advertising parameters for SIG Mesh");
    this->advertising_active_ = true;
  }

  if (this->enable_proxy_) {
    ESP_LOGI(TAG, "Configuring BTstack GATT Mesh Proxy Service (UUID 0x1828)...");
    this->proxy_server_.is_active = true;
    this->proxy_server_.service_uuid = MESH_PROXY_SERVICE_UUID;
    this->proxy_server_.data_in_uuid = MESH_PROXY_DATA_IN_UUID;
    this->proxy_server_.data_out_uuid = MESH_PROXY_DATA_OUT_UUID;
    ESP_LOGI(TAG, "Instantiated BTstack GATT Mesh Proxy Service 0x1828 with Data In (0x2ADE) and Data Out (0x2ADF)");
  }
}

void RP2040BluetoothSIGMesh::register_btstack_models_() {
  ESP_LOGI(TAG, "Registering BTstack Mesh Elements and Models...");
}

void RP2040BluetoothSIGMesh::process_mesh_pdu(const uint8_t *data, size_t len) {
  BluetoothSIGMesh::process_mesh_pdu(data, len);
}

void RP2040BluetoothSIGMesh::send_mesh_pdu(uint16_t dst, uint16_t app_idx, uint16_t opcode, const uint8_t *payload,
                                           size_t len) {
  BluetoothSIGMesh::send_mesh_pdu(dst, app_idx, opcode, payload, len);
  const auto &framed_pdu = this->get_last_outgoing_frame();
  ESP_LOGI(TAG, "Broadcasting RP2040 Pico W BLE Mesh advertisement packet (DST: 0x%04X, Framed Len: %zu)...", dst,
           framed_pdu.size());

  uint8_t raw_adv[31] = {0};
  raw_adv[0] = 0x02;  // Length
  raw_adv[1] = 0x01;  // Flags
  raw_adv[2] = 0x06;  // General Discoverable & BR/EDR Not Supported

  if (!framed_pdu.empty() && framed_pdu.size() <= 26) {
    raw_adv[3] = static_cast<uint8_t>(framed_pdu.size() + 1);
    raw_adv[4] = MESH_AD_TYPE_MESSAGE;  // 0x2A Mesh Message AD Type
    std::memcpy(raw_adv + 5, framed_pdu.data(), framed_pdu.size());

    gap_advertisements_set_data(framed_pdu.size() + 5, raw_adv);
    gap_advertisements_enable(1);
  }
}

}  // namespace bluetooth_sig_mesh
}  // namespace esphome

#endif  // USE_BLUETOOTH_SIG_MESH && USE_RP2040
