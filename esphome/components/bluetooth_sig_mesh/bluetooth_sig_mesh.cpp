#include "bluetooth_sig_mesh.h"

#ifdef USE_BLUETOOTH_SIG_MESH

#include <cstdio>
#include <cstdlib>

namespace esphome {
namespace bluetooth_sig_mesh {

static const char *const TAG = "bluetooth_sig_mesh";

BluetoothSIGMesh *global_bluetooth_sig_mesh = nullptr;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

bool BluetoothSIGMesh::parse_hex_key_(const std::string &hex, MeshKey &out_key) {
  if (hex.length() != MESH_KEY_SIZE * 2) {
    ESP_LOGE(TAG, "Invalid key hex length: %zu (expected %zu)", hex.length(), MESH_KEY_SIZE * 2);
    return false;
  }
  for (size_t i = 0; i < MESH_KEY_SIZE; i++) {
    std::string byte_str = hex.substr(i * 2, 2);
    out_key.bytes[i] = static_cast<uint8_t>(std::strtoul(byte_str.c_str(), nullptr, 16));
  }
  out_key.is_set = true;
  return true;
}

void BluetoothSIGMesh::set_net_key(const std::string &net_key_hex) {
  if (this->parse_hex_key_(net_key_hex, this->net_key_)) {
    ESP_LOGI(TAG, "Network key configured successfully");
    if (this->app_key_.is_set) {
      this->provision_state_ = ProvisioningState::PROVISIONED;
    }
  }
}

void BluetoothSIGMesh::set_app_key(const std::string &app_key_hex) {
  if (this->parse_hex_key_(app_key_hex, this->app_key_)) {
    ESP_LOGI(TAG, "Application key configured successfully");
    if (this->net_key_.is_set) {
      this->provision_state_ = ProvisioningState::PROVISIONED;
    }
  }
}

void BluetoothSIGMesh::setup() {
  global_bluetooth_sig_mesh = this;
  ESP_LOGCONFIG(TAG, "Setting up Bluetooth SIG Mesh...");
  if (this->net_key_.is_set && this->app_key_.is_set) {
    this->provision_state_ = ProvisioningState::PROVISIONED;
  }
}

void BluetoothSIGMesh::loop() {
  // Main loop processing for mesh PDU routing and proxy state updates
}

void BluetoothSIGMesh::dump_config() {
  ESP_LOGCONFIG(TAG, "Bluetooth SIG Mesh:");
  ESP_LOGCONFIG(TAG, "  Node enabled: %s", YESNO(this->enable_node_));
  ESP_LOGCONFIG(TAG, "  Proxy enabled: %s", YESNO(this->enable_proxy_));
  ESP_LOGCONFIG(TAG, "  Unicast address: 0x%04X", this->unicast_address_);
  ESP_LOGCONFIG(TAG, "  NetKey set: %s", YESNO(this->net_key_.is_set));
  ESP_LOGCONFIG(TAG, "  AppKey set: %s", YESNO(this->app_key_.is_set));
  ESP_LOGCONFIG(TAG, "  Provisioning state: %s",
                this->provision_state_ == ProvisioningState::PROVISIONED ? "Provisioned" : "Unprovisioned");
}

void BluetoothSIGMesh::process_mesh_pdu(const uint8_t *data, size_t len) {
  if (len < 1 || data == nullptr) {
    return;
  }
  ESP_LOGVV(TAG, "Processing SIG Mesh PDU of length %zu", len);
}

void BluetoothSIGMesh::send_mesh_pdu(uint16_t dst, uint16_t app_idx, const uint8_t *payload, size_t len) {
  ESP_LOGD(TAG, "Sending SIG Mesh PDU to 0x%04X, app_idx: 0x%04X, len: %zu", dst, app_idx, len);
}

void BluetoothSIGMesh::on_generic_onoff_get(uint16_t src, uint16_t dst) {
  ESP_LOGD(TAG, "Generic OnOff Get received from 0x%04X to 0x%04X", src, dst);
}

void BluetoothSIGMesh::on_generic_onoff_set(uint16_t src, uint16_t dst, bool state, bool ack) {
  ESP_LOGD(TAG, "Generic OnOff Set received from 0x%04X to 0x%04X: state=%s, ack=%s", src, dst, YESNO(state),
           YESNO(ack));
}

}  // namespace bluetooth_sig_mesh
}  // namespace esphome

#endif  // USE_BLUETOOTH_SIG_MESH
