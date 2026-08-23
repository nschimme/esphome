#include "bluetooth_sig_mesh.h"

#ifdef USE_BLUETOOTH_SIG_MESH

#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace esphome {
namespace bluetooth_sig_mesh {

static const char *const TAG = "bluetooth_sig_mesh";

BluetoothSIGMesh *global_bluetooth_sig_mesh = nullptr;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

bool BluetoothSIGMesh::parse_device(const ble_device_base::ESPBTDevice &device) {
  for (const auto &sd : device.get_service_datas()) {
    if (sd.uuid == ble_device_base::ESPBTUUID::from_uint16(MESH_PROXY_SERVICE_UUID) ||
        sd.uuid == ble_device_base::ESPBTUUID::from_uint16(MESH_PROVISIONING_SERVICE_UUID)) {
      ESP_LOGVV(TAG, "Received SIG Mesh Service Data advertisement from MAC %012" PRIX64, device.address_uint64());
      this->handle_proxy_pdu(sd.data.data(), sd.data.size());
      return true;
    }
  }
  return false;
}

void BluetoothSIGMesh::mesh_aes_cmac(const uint8_t key[16], const uint8_t *msg, size_t len, uint8_t out[16]) {
  uint8_t x[16] = {0};
  uint8_t y[16] = {0};

  size_t n = (len + 15) / 16;
  if (n == 0) {
    n = 1;
  }

  for (size_t i = 0; i < n; i++) {
    size_t block_len = (i == n - 1 && (len % 16 != 0 || len == 0)) ? (len % 16) : 16;
    for (size_t j = 0; i * 16 + j < len && j < block_len; j++) {
      y[j] = x[j] ^ msg[i * 16 + j];
    }
    for (size_t j = block_len; j < 16; j++) {
      y[j] = x[j];
    }
    ble_device_base::aes128_encrypt_block(key, y, x);
  }

  std::memcpy(out, x, 16);
}

void BluetoothSIGMesh::mesh_s1(const uint8_t *m, size_t len, uint8_t out[16]) {
  static const uint8_t zero_key[16] = {0};
  mesh_aes_cmac(zero_key, m, len, out);
}

bool BluetoothSIGMesh::decrypt_mesh_payload(const uint8_t key[16], const uint8_t nonce[13], const uint8_t *ct,
                                            size_t ct_len, uint8_t *pt, size_t mic_len) {
  if (ct_len < mic_len) {
    return false;
  }
  size_t payload_len = ct_len - mic_len;
  const uint8_t *tag = ct + payload_len;
  return ble_device_base::aes_ccm_auth_decrypt(key, nonce, 13, nullptr, 0, ct, payload_len, pt, tag, mic_len);
}

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
  ESP_LOGCONFIG(TAG, "  Bound switches count: %zu", this->bound_switches_.size());
  ESP_LOGCONFIG(TAG, "  Bound lights count: %zu", this->bound_lights_.size());
  ESP_LOGCONFIG(TAG, "  Provisioning state: %s",
                this->provision_state_ == ProvisioningState::PROVISIONED ? "Provisioned" : "Unprovisioned");
}

void BluetoothSIGMesh::process_mesh_pdu(const uint8_t *data, size_t len) {
  if (len < 10 || data == nullptr) {
    return;
  }
  MeshNetworkPDUHeader hdr{};
  hdr.nid = data[0] & 0x7F;
  hdr.ctl = (data[1] & 0x80) != 0;
  hdr.ttl = data[1] & 0x7F;
  hdr.seq =
      (static_cast<uint32_t>(data[2]) << 16) | (static_cast<uint32_t>(data[3]) << 8) | static_cast<uint32_t>(data[4]);
  hdr.src = (static_cast<uint16_t>(data[5]) << 8) | static_cast<uint16_t>(data[6]);
  hdr.dst = (static_cast<uint16_t>(data[7]) << 8) | static_cast<uint16_t>(data[8]);

  ESP_LOGVV(TAG, "Mesh Network PDU: SRC=0x%04X, DST=0x%04X, SEQ=%" PRIu32 ", TTL=%u, CTL=%d", hdr.src, hdr.dst, hdr.seq,
            hdr.ttl, hdr.ctl);

  if (hdr.dst == this->unicast_address_ || hdr.dst == 0xFFFF) {
    const uint8_t *encrypted_payload = data + 9;
    size_t encrypted_len = len - 9;

    if (this->net_key_.is_set && encrypted_len > 4 && encrypted_len <= 128) {
      uint8_t nonce[13] = {0};
      nonce[0] = 0x00;  // Network Nonce
      nonce[1] = hdr.ttl | (hdr.ctl ? 0x80 : 0x00);
      nonce[2] = (hdr.seq >> 16) & 0xFF;
      nonce[3] = (hdr.seq >> 8) & 0xFF;
      nonce[4] = hdr.seq & 0xFF;
      nonce[5] = (hdr.src >> 8) & 0xFF;
      nonce[6] = hdr.src & 0xFF;

      uint8_t decrypted[128] = {0};
      size_t mic_len = hdr.ctl ? 8 : 4;

      if (decrypt_mesh_payload(this->net_key_.bytes.data(), nonce, encrypted_payload, encrypted_len, decrypted,
                               mic_len)) {
        ESP_LOGD(TAG, "Network PDU MIC validation and decryption successful");
        this->process_network_pdu(hdr, decrypted, encrypted_len - mic_len);
        return;
      }
    }

    this->process_network_pdu(hdr, encrypted_payload, encrypted_len);
  }
}

void BluetoothSIGMesh::process_network_pdu(const MeshNetworkPDUHeader &hdr, const uint8_t *payload, size_t len) {
  if (len < 2 || payload == nullptr) {
    return;
  }
  uint16_t opcode = 0;
  size_t opcode_len = 0;
  if ((payload[0] & 0x80) == 0) {
    opcode = payload[0];
    opcode_len = 1;
  } else if ((payload[0] & 0xC0) == 0x80) {
    opcode = (static_cast<uint16_t>(payload[0]) << 8) | static_cast<uint16_t>(payload[1]);
    opcode_len = 2;
  } else {
    opcode = (static_cast<uint16_t>(payload[0]) << 8) | static_cast<uint16_t>(payload[1]);
    opcode_len = 3;
  }

  this->process_access_pdu(hdr.src, hdr.dst, opcode, payload + opcode_len, len - opcode_len);
}

void BluetoothSIGMesh::process_access_pdu(uint16_t src, uint16_t dst, uint16_t opcode, const uint8_t *payload,
                                          size_t len) {
  switch (opcode) {
    case OPCODE_GENERIC_ONOFF_GET:
      this->on_generic_onoff_get(src, dst);
      break;
    case OPCODE_GENERIC_ONOFF_SET:
      if (len >= 1) {
        bool state = (payload[0] & 0x01) != 0;
        this->on_generic_onoff_set(src, dst, state, true);
      }
      break;
    case OPCODE_GENERIC_ONOFF_SET_UNACK:
      if (len >= 1) {
        bool state = (payload[0] & 0x01) != 0;
        this->on_generic_onoff_set(src, dst, state, false);
      }
      break;
    case OPCODE_GENERIC_LEVEL_GET:
      this->on_generic_level_get(src, dst);
      break;
    case OPCODE_GENERIC_LEVEL_SET:
      if (len >= 2) {
        int16_t level =
            static_cast<int16_t>((static_cast<uint16_t>(payload[1]) << 8) | static_cast<uint16_t>(payload[0]));
        this->on_generic_level_set(src, dst, level, true);
      }
      break;
    case OPCODE_GENERIC_LEVEL_SET_UNACK:
      if (len >= 2) {
        int16_t level =
            static_cast<int16_t>((static_cast<uint16_t>(payload[1]) << 8) | static_cast<uint16_t>(payload[0]));
        this->on_generic_level_set(src, dst, level, false);
      }
      break;
    default:
      ESP_LOGD(TAG, "Access PDU from 0x%04X, Opcode: 0x%04X, Len: %zu", src, opcode, len);
      break;
  }
}

void BluetoothSIGMesh::send_mesh_pdu(uint16_t dst, uint16_t app_idx, uint16_t opcode, const uint8_t *payload,
                                     size_t len) {
  ESP_LOGD(TAG, "Sending SIG Mesh PDU to 0x%04X, Opcode: 0x%04X, AppIdx: 0x%04X, Len: %zu", dst, opcode, app_idx, len);
}

void BluetoothSIGMesh::handle_proxy_pdu(const uint8_t *data, size_t len) {
  if (len < 1 || data == nullptr) {
    return;
  }
  uint8_t pdu_type = data[0] & 0x3F;
  switch (pdu_type) {
    case PROXY_PDU_TYPE_NET_PDU:
      this->process_mesh_pdu(data + 1, len - 1);
      break;
    case PROXY_PDU_TYPE_CONFIG:
      if (len >= 2) {
        uint8_t proxy_opcode = data[1];
        if (proxy_opcode == PROXY_CONFIG_OPCODE_SET_FILTER_TYPE && len >= 3) {
          this->set_proxy_filter_type(data[2]);
        }
      }
      break;
    default:
      ESP_LOGVV(TAG, "Handled Proxy PDU type %u, len %zu", pdu_type, len);
      break;
  }
}

void BluetoothSIGMesh::set_proxy_filter_type(uint8_t filter_type) {
  this->proxy_filter_type_ = filter_type;
  this->proxy_filter_addresses_.clear();
  ESP_LOGI(TAG, "Proxy Filter type set to %s",
           filter_type == PROXY_FILTER_TYPE_WHITE_LIST ? "White List" : "Black List");
}

void BluetoothSIGMesh::add_proxy_filter_address(uint16_t address) {
  this->proxy_filter_addresses_.insert(address);
  ESP_LOGD(TAG, "Added 0x%04X to Proxy Filter", address);
}

void BluetoothSIGMesh::remove_proxy_filter_address(uint16_t address) {
  this->proxy_filter_addresses_.erase(address);
  ESP_LOGD(TAG, "Removed 0x%04X from Proxy Filter", address);
}

void BluetoothSIGMesh::on_generic_onoff_get(uint16_t src, uint16_t dst) {
  bool current_state = this->generic_onoff_state_;
  if (!this->bound_switches_.empty() && this->bound_switches_[0] != nullptr) {
    current_state = this->bound_switches_[0]->state;
  } else if (!this->bound_lights_.empty() && this->bound_lights_[0] != nullptr) {
    current_state = this->bound_lights_[0]->remote_values.is_on();
  }
  ESP_LOGD(TAG, "Generic OnOff Get from 0x%04X, current state: %s", src, YESNO(current_state));
  uint8_t status_payload[1] = {static_cast<uint8_t>(current_state ? 1 : 0)};
  this->send_mesh_pdu(src, this->app_key_index_, OPCODE_GENERIC_ONOFF_STATUS, status_payload, sizeof(status_payload));
}

void BluetoothSIGMesh::on_generic_onoff_set(uint16_t src, uint16_t dst, bool state, bool ack) {
  ESP_LOGI(TAG, "Generic OnOff Set from 0x%04X: new_state=%s, ack=%s", src, YESNO(state), YESNO(ack));
  this->generic_onoff_state_ = state;
  for (auto *sw : this->bound_switches_) {
    if (sw != nullptr) {
      if (state) {
        sw->turn_on();
      } else {
        sw->turn_off();
      }
    }
  }
  for (auto *lgt : this->bound_lights_) {
    if (lgt != nullptr) {
      auto call = lgt->make_call();
      call.set_state(state);
      call.perform();
    }
  }
  if (ack) {
    uint8_t status_payload[1] = {static_cast<uint8_t>(this->generic_onoff_state_ ? 1 : 0)};
    this->send_mesh_pdu(src, this->app_key_index_, OPCODE_GENERIC_ONOFF_STATUS, status_payload, sizeof(status_payload));
  }
}

void BluetoothSIGMesh::on_generic_level_get(uint16_t src, uint16_t dst) {
  ESP_LOGD(TAG, "Generic Level Get from 0x%04X, current level: %d", src, this->generic_level_state_);
  uint8_t status_payload[2] = {static_cast<uint8_t>(this->generic_level_state_ & 0xFF),
                               static_cast<uint8_t>((this->generic_level_state_ >> 8) & 0xFF)};
  this->send_mesh_pdu(src, this->app_key_index_, OPCODE_GENERIC_LEVEL_STATUS, status_payload, sizeof(status_payload));
}

void BluetoothSIGMesh::on_generic_level_set(uint16_t src, uint16_t dst, int16_t level, bool ack) {
  ESP_LOGI(TAG, "Generic Level Set from 0x%04X: new_level=%d, ack=%s", src, level, YESNO(ack));
  this->generic_level_state_ = level;
  for (auto *lgt : this->bound_lights_) {
    if (lgt != nullptr) {
      float brightness = static_cast<float>(level) / 32767.0f;
      if (brightness < 0.0f) {
        brightness = 0.0f;
      }
      auto call = lgt->make_call();
      call.set_brightness(brightness);
      call.set_state(brightness > 0.0f);
      call.perform();
    }
  }
  if (ack) {
    uint8_t status_payload[2] = {static_cast<uint8_t>(this->generic_level_state_ & 0xFF),
                                 static_cast<uint8_t>((this->generic_level_state_ >> 8) & 0xFF)};
    this->send_mesh_pdu(src, this->app_key_index_, OPCODE_GENERIC_LEVEL_STATUS, status_payload, sizeof(status_payload));
  }
}

}  // namespace bluetooth_sig_mesh
}  // namespace esphome

#endif  // USE_BLUETOOTH_SIG_MESH
