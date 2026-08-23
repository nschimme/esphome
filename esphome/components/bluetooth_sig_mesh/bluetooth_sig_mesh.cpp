#include "bluetooth_sig_mesh.h"

#ifdef USE_BLUETOOTH_SIG_MESH

#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace esphome {
namespace bluetooth_sig_mesh {

static const char *const TAG = "bluetooth_sig_mesh";

BluetoothSIGMesh *global_bluetooth_sig_mesh = nullptr;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

void BluetoothSIGMesh::encrypt_mesh_payload(const uint8_t key[16], const uint8_t nonce[13], const uint8_t *pt,
                                            size_t pt_len, uint8_t *ct, size_t mic_len) {
  uint8_t tag[8] = {0};
  ble_device_base::aes_ccm_auth_encrypt(key, nonce, 13, nullptr, 0, pt, pt_len, ct, tag, mic_len);
  std::memcpy(ct + pt_len, tag, mic_len);
}

void BluetoothSIGMesh::on_proxy_data_in_write(const uint8_t *data, size_t len) {
  ESP_LOGD(TAG, "GATT Proxy Data In (0x2ADE) write received, len: %zu", len);
  this->handle_proxy_pdu(data, len);
}

bool BluetoothSIGMesh::parse_device(const ble_device_base::ESPBTDevice &device) {
  for (const auto &sd : device.get_service_datas()) {
    if (sd.uuid == ble_device_base::ESPBTUUID::from_uint16(MESH_PROXY_SERVICE_UUID) ||
        sd.uuid == ble_device_base::ESPBTUUID::from_uint16(MESH_PROVISIONING_SERVICE_UUID)) {
      ESP_LOGVV(TAG, "Received SIG Mesh Service Data advertisement from MAC %012" PRIX64, device.address_uint64());
      this->handle_proxy_pdu(sd.data.data(), sd.data.size());
      return true;
    }
  }

  for (const auto &md : device.get_manufacturer_datas()) {
    if (md.data.size() >= 2) {
      uint8_t ad_type = md.data[0];
      if (ad_type == MESH_AD_TYPE_MESSAGE) {  // 0x2A Mesh Message
        ESP_LOGVV(TAG, "Received Mesh Message AD Type 0x2A from MAC %012" PRIX64, device.address_uint64());
        this->process_mesh_pdu(md.data.data() + 1, md.data.size() - 1);
        return true;
      } else if (ad_type == MESH_AD_TYPE_BEACON) {  // 0x2B Mesh Beacon
        ESP_LOGVV(TAG, "Received Mesh Beacon AD Type 0x2B from MAC %012" PRIX64, device.address_uint64());
        return true;
      } else if (ad_type == MESH_AD_TYPE_PB_ADV) {  // 0x29 PB-ADV
        ESP_LOGVV(TAG, "Received PB-ADV AD Type 0x29 from MAC %012" PRIX64, device.address_uint64());
        return true;
      }
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

void BluetoothSIGMesh::mesh_k1(const uint8_t n[16], const uint8_t *p, size_t p_len, uint8_t out[16]) {
  static const uint8_t salt_smk1[4] = {'s', 'm', 'k', '1'};
  uint8_t salt[16] = {0};
  mesh_s1(salt_smk1, sizeof(salt_smk1), salt);

  uint8_t t[16] = {0};
  mesh_aes_cmac(salt, n, 16, t);
  mesh_aes_cmac(t, p, p_len, out);
}

void BluetoothSIGMesh::mesh_k2(const uint8_t net_key[16], const uint8_t *p, size_t p_len, uint8_t *out_nid,
                               uint8_t out_ek[16], uint8_t out_pk[16]) {
  static const uint8_t salt_smk2[4] = {'s', 'm', 'k', '2'};
  uint8_t salt[16] = {0};
  mesh_s1(salt_smk2, sizeof(salt_smk2), salt);

  uint8_t t[16] = {0};
  mesh_aes_cmac(salt, net_key, 16, t);

  std::vector<uint8_t> msg1(p_len + 1);
  if (p_len > 0 && p != nullptr) {
    std::memcpy(msg1.data(), p, p_len);
  }
  msg1[p_len] = 0x01;
  uint8_t t1[16] = {0};
  mesh_aes_cmac(t, msg1.data(), msg1.size(), t1);
  if (out_nid != nullptr) {
    *out_nid = t1[15] & 0x7F;
  }

  std::vector<uint8_t> msg2(16 + p_len + 1);
  std::memcpy(msg2.data(), t1, 16);
  if (p_len > 0 && p != nullptr) {
    std::memcpy(msg2.data() + 16, p, p_len);
  }
  msg2[16 + p_len] = 0x02;
  mesh_aes_cmac(t, msg2.data(), msg2.size(), out_ek);

  std::vector<uint8_t> msg3(16 + p_len + 1);
  std::memcpy(msg3.data(), out_ek, 16);
  if (p_len > 0 && p != nullptr) {
    std::memcpy(msg3.data() + 16, p, p_len);
  }
  msg3[16 + p_len] = 0x03;
  mesh_aes_cmac(t, msg3.data(), msg3.size(), out_pk);
}

void BluetoothSIGMesh::obfuscate_header(const uint8_t privacy_key[16], uint32_t iv_index,
                                        const uint8_t privacy_random[7], uint8_t header_data[6]) {
  uint8_t privacy_block_in[16] = {0};
  privacy_block_in[0] = 0x00;
  privacy_block_in[1] = 0x00;
  privacy_block_in[2] = 0x00;
  privacy_block_in[3] = 0x00;
  privacy_block_in[4] = 0x00;
  privacy_block_in[5] = (iv_index >> 24) & 0xFF;
  privacy_block_in[6] = (iv_index >> 16) & 0xFF;
  privacy_block_in[7] = (iv_index >> 8) & 0xFF;
  privacy_block_in[8] = iv_index & 0xFF;
  std::memcpy(privacy_block_in + 9, privacy_random, 7);

  uint8_t privacy_block[16] = {0};
  ble_device_base::aes128_encrypt_block(privacy_key, privacy_block_in, privacy_block);

  for (size_t i = 0; i < 6; i++) {
    header_data[i] ^= privacy_block[i];
  }
}

void BluetoothSIGMesh::derive_net_keys_() {
  if (this->net_key_.is_set) {
    static const uint8_t p[1] = {0x00};
    mesh_k2(this->net_key_.bytes.data(), p, 1, &this->nid_, this->encryption_key_, this->privacy_key_);
    ESP_LOGI(TAG, "Derived NetKey parameters: NID=0x%02X", this->nid_);
  }
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
    this->derive_net_keys_();
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
  if (this->enable_proxy_) {
    this->proxy_server_.is_active = true;
    ESP_LOGI(TAG, "Activated GATT Mesh Proxy Server Service (0x1828)");
  }
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
  ESP_LOGCONFIG(TAG, "  Proxy enabled: %s (GATT Server: %s)", YESNO(this->enable_proxy_),
                YESNO(this->proxy_server_.is_active));
  ESP_LOGCONFIG(TAG, "  Unicast address: 0x%04X", this->unicast_address_);
  ESP_LOGCONFIG(TAG, "  NetKey set: %s (NID: 0x%02X)", YESNO(this->net_key_.is_set), this->nid_);
  ESP_LOGCONFIG(TAG, "  AppKey set: %s", YESNO(this->app_key_.is_set));
  ESP_LOGCONFIG(TAG, "  Bound switches count: %zu", this->bound_switches_.size());
  ESP_LOGCONFIG(TAG, "  Bound lights count: %zu", this->bound_lights_.size());
  ESP_LOGCONFIG(TAG, "  Provisioning state: %s",
                this->provision_state_ == ProvisioningState::PROVISIONED ? "Provisioned" : "Unprovisioned");
}

void BluetoothSIGMesh::process_mesh_pdu(const uint8_t *data, size_t len) {
  if (len < 14 || data == nullptr) {
    return;
  }

  uint8_t header_copy[6] = {0};
  std::memcpy(header_copy, data + 1, 6);

  if (this->net_key_.is_set) {
    // Privacy Random is bytes 7..13 of the Network PDU
    obfuscate_header(this->privacy_key_, this->iv_index_, data + 7, header_copy);
  }

  MeshNetworkPDUHeader hdr{};
  hdr.nid = data[0] & 0x7F;
  hdr.ctl = (header_copy[0] & 0x80) != 0;
  hdr.ttl = header_copy[0] & 0x7F;
  hdr.seq = (static_cast<uint32_t>(header_copy[1]) << 16) | (static_cast<uint32_t>(header_copy[2]) << 8) |
            static_cast<uint32_t>(header_copy[3]);
  hdr.src = (static_cast<uint16_t>(header_copy[4]) << 8) | static_cast<uint16_t>(header_copy[5]);

  const uint8_t *encrypted_pdu = data + 7;
  size_t encrypted_len = len - 7;

  if (this->net_key_.is_set && encrypted_len >= 6 && encrypted_len <= 128) {
    uint8_t nonce[13] = {0};
    nonce[0] = 0x00;  // Network Nonce
    nonce[1] = (hdr.ctl ? 0x80 : 0x00) | (hdr.ttl & 0x7F);
    nonce[2] = (hdr.seq >> 16) & 0xFF;
    nonce[3] = (hdr.seq >> 8) & 0xFF;
    nonce[4] = hdr.seq & 0xFF;
    nonce[5] = (hdr.src >> 8) & 0xFF;
    nonce[6] = hdr.src & 0xFF;
    nonce[7] = 0x00;  // Pad
    nonce[8] = 0x00;
    nonce[9] = (this->iv_index_ >> 24) & 0xFF;
    nonce[10] = (this->iv_index_ >> 16) & 0xFF;
    nonce[11] = (this->iv_index_ >> 8) & 0xFF;
    nonce[12] = this->iv_index_ & 0xFF;

    uint8_t decrypted[128] = {0};
    size_t mic_len = hdr.ctl ? 8 : 4;

    if (decrypt_mesh_payload(this->encryption_key_, nonce, encrypted_pdu, encrypted_len, decrypted, mic_len)) {
      hdr.dst = (static_cast<uint16_t>(decrypted[0]) << 8) | static_cast<uint16_t>(decrypted[1]);
      ESP_LOGD(TAG, "Network PDU de-obfuscated and decrypted: SRC=0x%04X, DST=0x%04X, SEQ=%" PRIu32, hdr.src, hdr.dst,
               hdr.seq);

      if (hdr.dst == this->unicast_address_ || hdr.dst == 0xFFFF) {
        size_t transport_pdu_len = encrypted_len - mic_len - 2;
        this->process_network_pdu(hdr, decrypted + 2, transport_pdu_len);
      }
      return;
    }
  }

  hdr.dst = (static_cast<uint16_t>(data[7]) << 8) | static_cast<uint16_t>(data[8]);
  if (hdr.dst == this->unicast_address_ || hdr.dst == 0xFFFF) {
    this->process_network_pdu(hdr, data + 9, len - 9);
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
  uint32_t seq = this->seq_number_++;
  ESP_LOGD(TAG, "Framing outgoing SIG Mesh PDU: DST=0x%04X, SEQ=%" PRIu32 ", Opcode=0x%04X, Len=%zu", dst, seq, opcode,
           len);

  uint8_t pdu_buffer[31] = {0};
  pdu_buffer[0] = this->nid_ & 0x7F;
  pdu_buffer[1] = 0x07;  // TTL 7
  pdu_buffer[2] = (seq >> 16) & 0xFF;
  pdu_buffer[3] = (seq >> 8) & 0xFF;
  pdu_buffer[4] = seq & 0xFF;
  pdu_buffer[5] = (this->unicast_address_ >> 8) & 0xFF;
  pdu_buffer[6] = this->unicast_address_ & 0xFF;

  uint8_t plaintext_payload[20] = {0};
  plaintext_payload[0] = (dst >> 8) & 0xFF;
  plaintext_payload[1] = dst & 0xFF;

  size_t pt_offset = 2;
  if (opcode > 0xFF) {
    plaintext_payload[pt_offset++] = (opcode >> 8) & 0xFF;
    plaintext_payload[pt_offset++] = opcode & 0xFF;
  } else {
    plaintext_payload[pt_offset++] = opcode & 0xFF;
  }

  if (payload != nullptr && len > 0 && pt_offset + len <= sizeof(plaintext_payload)) {
    std::memcpy(plaintext_payload + pt_offset, payload, len);
    pt_offset += len;
  }

  size_t mic_len = 4;
  uint8_t nonce[13] = {0};
  nonce[0] = 0x00;  // Network Nonce
  nonce[1] = 0x07;  // TTL 7
  nonce[2] = (seq >> 16) & 0xFF;
  nonce[3] = (seq >> 8) & 0xFF;
  nonce[4] = seq & 0xFF;
  nonce[5] = (this->unicast_address_ >> 8) & 0xFF;
  nonce[6] = this->unicast_address_ & 0xFF;
  nonce[7] = 0x00;
  nonce[8] = 0x00;
  nonce[9] = (this->iv_index_ >> 24) & 0xFF;
  nonce[10] = (this->iv_index_ >> 16) & 0xFF;
  nonce[11] = (this->iv_index_ >> 8) & 0xFF;
  nonce[12] = this->iv_index_ & 0xFF;

  if (this->net_key_.is_set) {
    encrypt_mesh_payload(this->encryption_key_, nonce, plaintext_payload, pt_offset, pdu_buffer + 7, mic_len);
    size_t encrypted_frame_len = 7 + pt_offset + mic_len;

    uint8_t header_to_obfuscate[6] = {pdu_buffer[1], pdu_buffer[2], pdu_buffer[3],
                                      pdu_buffer[4], pdu_buffer[5], pdu_buffer[6]};
    obfuscate_header(this->privacy_key_, this->iv_index_, pdu_buffer + 7, header_to_obfuscate);
    std::memcpy(pdu_buffer + 1, header_to_obfuscate, 6);

    this->last_outgoing_frame_.assign(pdu_buffer, pdu_buffer + encrypted_frame_len);
  } else {
    std::memcpy(pdu_buffer + 7, plaintext_payload, pt_offset);
    this->last_outgoing_frame_.assign(pdu_buffer, pdu_buffer + 7 + pt_offset);
  }
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
