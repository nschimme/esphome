#include "rp2040.h"

#if defined(USE_BLUETOOTH_SIG_MESH) && defined(USE_RP2040)

#include <btstack.h>
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
  ESP_LOGI(TAG, "Initializing BTstack SIG Mesh Node parameters...");
  this->adv_interval_min_ = 0x0020;  // 20ms
  this->adv_interval_max_ = 0x0040;  // 40ms

  if (rp2040_ble::global_rp2040_ble != nullptr) {
    gap_advertisements_set_params(this->adv_interval_min_, this->adv_interval_max_, ADV_NONCONN_IND, 0,
                                  nullptr, ADV_CHANNEL_ALL, ADV_FILTER_ALLOW_ALL);
    ESP_LOGD(TAG, "Configured RP2040 BTstack BLE GAP advertising parameters for SIG Mesh");
    this->advertising_active_ = true;
  }
}

void RP2040BluetoothSIGMesh::register_btstack_models_() {
  ESP_LOGI(TAG, "Registering BTstack Mesh Elements and Models...");
}

void RP2040BluetoothSIGMesh::process_mesh_pdu(const uint8_t *data, size_t len) {
  BluetoothSIGMesh::process_mesh_pdu(data, len);
}

void RP2040BluetoothSIGMesh::transmit_last_outgoing_frame() {
  const uint8_t *pdu_data = this->get_last_outgoing_frame_data();
  size_t pdu_len = this->get_last_outgoing_frame_len();
  if (pdu_len == 0 || pdu_data == nullptr || pdu_len > 26) {
    return;
  }

  this->raw_adv_buffer_.fill(0);
  this->raw_adv_buffer_[0] = 0x02;  // Length
  this->raw_adv_buffer_[1] = 0x01;  // Flags
  this->raw_adv_buffer_[2] = 0x06;  // General Discoverable & BR/EDR Not Supported
  this->raw_adv_buffer_[3] = static_cast<uint8_t>(pdu_len + 1);
  this->raw_adv_buffer_[4] = MESH_AD_TYPE_MESSAGE;  // 0x2A Mesh Message AD Type
  std::memcpy(this->raw_adv_buffer_.data() + 5, pdu_data, pdu_len);

  if (rp2040_ble::global_rp2040_ble != nullptr) {
    gap_advertisements_set_data(pdu_len + 5, this->raw_adv_buffer_.data());
    gap_advertisements_enable(1);
  }
}

void RP2040BluetoothSIGMesh::send_mesh_pdu(uint16_t dst, uint16_t app_idx, uint16_t opcode, const uint8_t *payload,
                                           size_t len) {
  BluetoothSIGMesh::send_mesh_pdu(dst, app_idx, opcode, payload, len);
  this->transmit_last_outgoing_frame();
}

}  // namespace bluetooth_sig_mesh
}  // namespace esphome

#endif  // USE_BLUETOOTH_SIG_MESH && USE_RP2040
