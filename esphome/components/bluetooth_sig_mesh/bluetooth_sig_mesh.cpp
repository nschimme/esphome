#include "bluetooth_sig_mesh.h"

#ifdef USE_BLUETOOTH_SIG_MESH

#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace esphome {
namespace bluetooth_sig_mesh {

static const char *const TAG = "bluetooth_sig_mesh";

BluetoothSIGMesh *global_bluetooth_sig_mesh = nullptr;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

uint8_t BluetoothSIGMesh::mesh_k4(const uint8_t app_key[16]) {
  // Pre-calculated salt_smk4 = s1("smk4")
  static const uint8_t salt_smk4[16] = {0x47, 0x14, 0xD4, 0xAA, 0xEB, 0x1F, 0xB6, 0xDF,
                                        0x10, 0xE9, 0xB4, 0x10, 0x14, 0x98, 0xBF, 0xA2};

  uint8_t t[16] = {0};
  mesh_aes_cmac(salt_smk4, app_key, 16, t);

  static const uint8_t msg[4] = {'i', 'd', '6', 0x01};
  uint8_t out[16] = {0};
  mesh_aes_cmac(t, msg, sizeof(msg), out);
  return out[15] & 0x3F;
}

void BluetoothSIGMesh::derive_app_keys_() {
  if (this->app_key_.is_set) {
    this->aid_ = mesh_k4(this->app_key_.bytes.data());
    ESP_LOGI(TAG, "Derived AppKey parameters: AID=0x%02X", this->aid_);
  }
}

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
    if (sd.uuid == ble_device_base::ESPBTUUID::from_uint16(MESH_AD_TYPE_MESSAGE)) {  // 0x2A Mesh Message
      ESP_LOGVV(TAG, "Received Mesh Message AD Type 0x2A from MAC %012" PRIX64, device.address_uint64());
      this->process_mesh_pdu(sd.data.data(), sd.data.size());
      return true;
    }
    if (sd.uuid == ble_device_base::ESPBTUUID::from_uint16(MESH_AD_TYPE_BEACON)) {  // 0x2B Mesh Beacon
      ESP_LOGI(TAG, "Unprovisioned Mesh Beacon (0x2B) discovered from MAC %012" PRIX64, device.address_uint64());
      return true;
    }
    if (sd.uuid == ble_device_base::ESPBTUUID::from_uint16(MESH_AD_TYPE_PB_ADV)) {  // 0x29 PB-ADV
      ESP_LOGI(TAG, "Unprovisioned PB-ADV (0x29) advertisement discovered from MAC %012" PRIX64,
               device.address_uint64());
      return true;
    }
  }

  return false;
}

static void generate_cmac_subkeys(const uint8_t key[16], uint8_t k1[16], uint8_t k2[16]) {
  static const uint8_t const_rb[16] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                       0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x87};
  uint8_t l[16] = {0};
  uint8_t zero[16] = {0};
  ble_device_base::aes128_encrypt_block(key, zero, l);

  // Derive K1
  bool carry = (l[0] & 0x80) != 0;
  for (int i = 0; i < 15; i++) {
    k1[i] = (l[i] << 1) | (l[i + 1] >> 7);
  }
  k1[15] = l[15] << 1;
  if (carry) {
    for (int i = 0; i < 16; i++) {
      k1[i] ^= const_rb[i];
    }
  }

  // Derive K2
  carry = (k1[0] & 0x80) != 0;
  for (int i = 0; i < 15; i++) {
    k2[i] = (k1[i] << 1) | (k1[i + 1] >> 7);
  }
  k2[15] = k1[15] << 1;
  if (carry) {
    for (int i = 0; i < 16; i++) {
      k2[i] ^= const_rb[i];
    }
  }
}

void BluetoothSIGMesh::mesh_aes_cmac(const uint8_t key[16], const uint8_t *msg, size_t len, uint8_t out[16]) {
  uint8_t k1[16] = {0};
  uint8_t k2[16] = {0};
  generate_cmac_subkeys(key, k1, k2);

  size_t n = (len + 15) / 16;
  bool flag = true;
  if (n == 0) {
    n = 1;
    flag = false;
  } else {
    flag = (len % 16 == 0);
  }

  uint8_t m_last[16] = {0};
  if (flag) {
    for (size_t i = 0; i < 16; i++) {
      m_last[i] = msg[(n - 1) * 16 + i] ^ k1[i];
    }
  } else {
    size_t last_block_len = len % 16;
    for (size_t i = 0; i < last_block_len; i++) {
      m_last[i] = msg[(n - 1) * 16 + i];
    }
    m_last[last_block_len] = 0x80;
    for (size_t i = last_block_len + 1; i < 16; i++) {
      m_last[i] = 0x00;
    }
    for (size_t i = 0; i < 16; i++) {
      m_last[i] ^= k2[i];
    }
  }

  uint8_t x[16] = {0};
  uint8_t y[16] = {0};

  for (size_t i = 0; i < n - 1; i++) {
    for (size_t j = 0; j < 16; j++) {
      y[j] = x[j] ^ msg[i * 16 + j];
    }
    ble_device_base::aes128_encrypt_block(key, y, x);
  }

  for (size_t j = 0; j < 16; j++) {
    y[j] = x[j] ^ m_last[j];
  }
  ble_device_base::aes128_encrypt_block(key, y, x);

  std::memcpy(out, x, 16);
}

void BluetoothSIGMesh::mesh_s1(const uint8_t *m, size_t len, uint8_t out[16]) {
  static const uint8_t zero_key[16] = {0};
  mesh_aes_cmac(zero_key, m, len, out);
}

void BluetoothSIGMesh::mesh_k1(const uint8_t n[16], const uint8_t *p, size_t p_len, uint8_t out[16]) {
  // Pre-calculated salt_smk1 = s1("smk1")
  static const uint8_t salt_smk1[16] = {0x2E, 0xB1, 0x11, 0xAC, 0x2A, 0x48, 0x06, 0xA2,
                                        0xC7, 0xA3, 0xD7, 0xDF, 0xF9, 0x1A, 0xEB, 0x31};

  uint8_t t[16] = {0};
  mesh_aes_cmac(salt_smk1, n, 16, t);
  mesh_aes_cmac(t, p, p_len, out);
}

void BluetoothSIGMesh::mesh_k2(const uint8_t net_key[16], const uint8_t *p, size_t p_len, uint8_t *out_nid,
                               uint8_t out_ek[16], uint8_t out_pk[16]) {
  // Pre-calculated salt_smk2 = s1("smk2")
  static const uint8_t salt_smk2[16] = {0x33, 0x82, 0x56, 0x1B, 0xAD, 0x82, 0x10, 0x7E,
                                        0xAD, 0x3B, 0xF9, 0x7E, 0x8D, 0xA9, 0xC4, 0x6E};

  uint8_t t[16] = {0};
  mesh_aes_cmac(salt_smk2, net_key, 16, t);

  uint8_t buf[33] = {0};
  if (p_len > 0 && p != nullptr && p_len <= 16) {
    std::memcpy(buf, p, p_len);
  }
  buf[p_len] = 0x01;
  uint8_t t1[16] = {0};
  mesh_aes_cmac(t, buf, p_len + 1, t1);
  if (out_nid != nullptr) {
    *out_nid = t1[15] & 0x7F;
  }

  std::memcpy(buf, t1, 16);
  if (p_len > 0 && p != nullptr && p_len <= 16) {
    std::memcpy(buf + 16, p, p_len);
  }
  buf[16 + p_len] = 0x02;
  mesh_aes_cmac(t, buf, 16 + p_len + 1, out_ek);

  std::memcpy(buf, out_ek, 16);
  if (p_len > 0 && p != nullptr && p_len <= 16) {
    std::memcpy(buf + 16, p, p_len);
  }
  buf[16 + p_len] = 0x03;
  mesh_aes_cmac(t, buf, 16 + p_len + 1, out_pk);
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

void BluetoothSIGMesh::add_remote_node(uint16_t address, const std::string &device_key_hex, const std::string &name) {
  MeshKey dev_key{};
  if (this->parse_hex_key_(device_key_hex, dev_key)) {
    ESP_LOGI(TAG, "Registered remote mesh device 0x%04X ('%s')", address, name.c_str());
  }
}

void BluetoothSIGMesh::set_app_key(const std::string &app_key_hex) {
  if (this->parse_hex_key_(app_key_hex, this->app_key_)) {
    ESP_LOGI(TAG, "Application key configured successfully");
    this->derive_app_keys_();
    if (this->net_key_.is_set) {
      this->provision_state_ = ProvisioningState::PROVISIONED;
    }
  }
}

void BluetoothSIGMesh::setup() {
  global_bluetooth_sig_mesh = this;
  ESP_LOGCONFIG(TAG, "Setting up Bluetooth SIG Mesh...");
  this->pref_ = global_preferences->make_preference<uint32_t>(fnv1a_hash("sig_mesh_seq"));
  uint32_t saved_seq = 0;
  if (this->pref_.load(&saved_seq)) {
    this->seq_number_ = saved_seq + 100;  // Skip ahead to ensure freshness across reboots
    ESP_LOGI(TAG, "Loaded saved Mesh Sequence Number: %" PRIu32, this->seq_number_);
  }

  if (this->net_key_.is_set && this->app_key_.is_set) {
    this->provision_state_ = ProvisioningState::PROVISIONED;
  }
}

void BluetoothSIGMesh::loop() {
  // Main loop processing for mesh PDU routing and proxy state updates
}

void BluetoothSIGMesh::dump_config() {
  ESP_LOGCONFIG(TAG, "Bluetooth SIG Mesh Node:");
  ESP_LOGCONFIG(TAG, "  Relay enabled: %s", YESNO(this->relay_enabled_));
  ESP_LOGCONFIG(TAG, "  Unicast address: 0x%04X", this->unicast_address_);
  ESP_LOGCONFIG(TAG, "  NetKey set: %s (NID: 0x%02X)", YESNO(this->net_key_.is_set), this->nid_);
  ESP_LOGCONFIG(TAG, "  AppKey set: %s (AID: 0x%02X)", YESNO(this->app_key_.is_set), this->aid_);
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

  if (this->net_key_.is_set) {
    if (encrypted_len < 6 || encrypted_len > 128) {
      ESP_LOGW(TAG, "Invalid Network PDU payload length: %zu", encrypted_len);
      return;
    }

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

      // Mesh Relay Engine: If Relay Feature is enabled and TTL > 1, decrement TTL and relay network PDU
      if (this->relay_enabled_ && hdr.ttl > 1 && hdr.src != this->unicast_address_ && len <= 31) {
        uint8_t retransmitted_pdu[31] = {0};
        std::memcpy(retransmitted_pdu, data, len);

        uint8_t relay_hdr[6] = {0};
        std::memcpy(relay_hdr, header_copy, 6);
        relay_hdr[0] = (relay_hdr[0] & 0x80) | ((hdr.ttl - 1) & 0x7F);  // Decremented TTL

        obfuscate_header(this->privacy_key_, this->iv_index_, data + 7, relay_hdr);
        std::memcpy(retransmitted_pdu + 1, relay_hdr, 6);

        ESP_LOGD(TAG, "Relaying Mesh PDU from 0x%04X to 0x%04X (Decremented TTL: %u)", hdr.src, hdr.dst, hdr.ttl - 1);
        std::memcpy(this->last_outgoing_frame_.data(), retransmitted_pdu, len);
        this->last_outgoing_frame_len_ = len;
      }
    } else {
      ESP_LOGW(TAG, "Network PDU MIC decryption failed from NID 0x%02X", hdr.nid);
    }
    return;
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

  // Lower Transport PDU parsing: byte 0 contains SEG (bit 7), AKF (bit 6), AID (bits 0..5)
  bool seg = (payload[0] & 0x80) != 0;
  bool akf = (payload[0] & 0x40) != 0;
  uint8_t aid = payload[0] & 0x3F;

  if (seg) {
    ESP_LOGV(TAG, "Segmented Transport PDU received (unsegmented supported)");
    return;
  }

  const uint8_t *upper_transport_pdu = payload + 1;
  size_t upper_transport_len = len - 1;

  uint8_t access_pdu[128] = {0};
  size_t access_pdu_len = 0;

  if (akf && this->app_key_.is_set) {
    if (upper_transport_len < 4) {
      return;
    }
    uint8_t app_nonce[13] = {0};
    app_nonce[0] = 0x01;  // Application Nonce
    app_nonce[1] = 0x00;  // ASZMIC = 0
    app_nonce[2] = (hdr.seq >> 16) & 0xFF;
    app_nonce[3] = (hdr.seq >> 8) & 0xFF;
    app_nonce[4] = hdr.seq & 0xFF;
    app_nonce[5] = (hdr.src >> 8) & 0xFF;
    app_nonce[6] = hdr.src & 0xFF;
    app_nonce[7] = (hdr.dst >> 8) & 0xFF;
    app_nonce[8] = hdr.dst & 0xFF;
    app_nonce[9] = (this->iv_index_ >> 24) & 0xFF;
    app_nonce[10] = (this->iv_index_ >> 16) & 0xFF;
    app_nonce[11] = (this->iv_index_ >> 8) & 0xFF;
    app_nonce[12] = this->iv_index_ & 0xFF;

    size_t mic_len = 4;
    if (!decrypt_mesh_payload(this->app_key_.bytes.data(), app_nonce, upper_transport_pdu, upper_transport_len,
                              access_pdu, mic_len)) {
      ESP_LOGW(TAG, "Upper transport decryption failed from SRC 0x%04X", hdr.src);
      return;
    }
    access_pdu_len = upper_transport_len - mic_len;
  } else {
    std::memcpy(access_pdu, upper_transport_pdu, upper_transport_len);
    access_pdu_len = upper_transport_len;
  }

  if (access_pdu_len < 1) {
    return;
  }

  uint16_t opcode = 0;
  size_t opcode_len = 0;
  if ((access_pdu[0] & 0x80) == 0) {
    opcode = access_pdu[0];
    opcode_len = 1;
  } else if ((access_pdu[0] & 0xC0) == 0x80) {
    opcode = (static_cast<uint16_t>(access_pdu[0]) << 8) | static_cast<uint16_t>(access_pdu[1]);
    opcode_len = 2;
  } else {
    opcode = (static_cast<uint16_t>(access_pdu[0]) << 8) | static_cast<uint16_t>(access_pdu[1]);
    opcode_len = 3;
  }

  this->process_access_pdu(hdr.src, hdr.dst, opcode, access_pdu + opcode_len, access_pdu_len - opcode_len);
}

void BluetoothSIGMesh::send_onoff(uint16_t dst, bool state, bool ack) {
  uint8_t payload[1] = {static_cast<uint8_t>(state ? 1 : 0)};
  uint16_t opcode = ack ? OPCODE_GENERIC_ONOFF_SET : OPCODE_GENERIC_ONOFF_SET_UNACK;
  this->send_mesh_pdu(dst, this->app_key_index_, opcode, payload, sizeof(payload));
}

void BluetoothSIGMesh::send_level(uint16_t dst, int16_t level, bool ack) {
  uint8_t payload[2] = {0};
  encode_int16_le(level, payload);
  uint16_t opcode = ack ? OPCODE_GENERIC_LEVEL_SET : OPCODE_GENERIC_LEVEL_SET_UNACK;
  this->send_mesh_pdu(dst, this->app_key_index_, opcode, payload, sizeof(payload));
}

void BluetoothSIGMesh::send_lightness(uint16_t dst, uint16_t lightness, bool ack) {
  uint8_t payload[2] = {0};
  encode_uint16_le(lightness, payload);
  uint16_t opcode = ack ? OPCODE_LIGHT_LIGHTNESS_SET : OPCODE_LIGHT_LIGHTNESS_SET_UNACK;
  this->send_mesh_pdu(dst, this->app_key_index_, opcode, payload, sizeof(payload));
}

void BluetoothSIGMesh::send_ctl(uint16_t dst, uint16_t lightness, uint16_t temperature, int16_t delta_uv, bool ack) {
  uint8_t payload[6] = {
      static_cast<uint8_t>(lightness & 0xFF), static_cast<uint8_t>((lightness >> 8) & 0xFF),
      static_cast<uint8_t>(temperature & 0xFF), static_cast<uint8_t>((temperature >> 8) & 0xFF),
      static_cast<uint8_t>(delta_uv & 0xFF), static_cast<uint8_t>((delta_uv >> 8) & 0xFF)};
  uint16_t opcode = ack ? OPCODE_LIGHT_CTL_SET : OPCODE_LIGHT_CTL_SET_UNACK;
  this->send_mesh_pdu(dst, this->app_key_index_, opcode, payload, sizeof(payload));
}

void BluetoothSIGMesh::send_hsl(uint16_t dst, uint16_t lightness, uint16_t hue, uint16_t saturation, bool ack) {
  uint8_t payload[6] = {
      static_cast<uint8_t>(lightness & 0xFF), static_cast<uint8_t>((lightness >> 8) & 0xFF),
      static_cast<uint8_t>(hue & 0xFF), static_cast<uint8_t>((hue >> 8) & 0xFF),
      static_cast<uint8_t>(saturation & 0xFF), static_cast<uint8_t>((saturation >> 8) & 0xFF)};
  uint16_t opcode = ack ? OPCODE_LIGHT_HSL_SET : OPCODE_LIGHT_HSL_SET_UNACK;
  this->send_mesh_pdu(dst, this->app_key_index_, opcode, payload, sizeof(payload));
}

void BluetoothSIGMesh::process_access_pdu(uint16_t src, uint16_t dst, uint16_t opcode, const uint8_t *payload,
                                          size_t len) {
  for (const auto &cb : this->node_seen_callbacks_) {
    cb(src, opcode, payload, len);
  }
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
    case OPCODE_LIGHT_CTL_SET:
    case OPCODE_LIGHT_CTL_SET_UNACK:
      if (len >= 4) {
        uint16_t lightness = static_cast<uint16_t>(payload[0]) | (static_cast<uint16_t>(payload[1]) << 8);
        uint16_t temp = static_cast<uint16_t>(payload[2]) | (static_cast<uint16_t>(payload[3]) << 8);
        for (auto *lgt : this->bound_lights_) {
          if (lgt != nullptr) {
            auto call = lgt->make_call();
            call.set_brightness(static_cast<float>(lightness) / 65535.0f);
            if (temp >= 800 && temp <= 20000) {
              float mireds = 1000000.0f / static_cast<float>(temp);
              call.set_color_temperature(mireds);
            }
            call.set_state(lightness > 0);
            call.perform();
          }
        }
        if (opcode == OPCODE_LIGHT_CTL_SET) {
          uint8_t status_payload[6] = {payload[0], payload[1], payload[2], payload[3], 0x00, 0x00};
          this->send_mesh_pdu(src, this->app_key_index_, OPCODE_LIGHT_CTL_STATUS, status_payload, sizeof(status_payload));
        }
      }
      break;
    case OPCODE_LIGHT_HSL_SET:
    case OPCODE_LIGHT_HSL_SET_UNACK:
      if (len >= 6) {
        uint16_t lightness = static_cast<uint16_t>(payload[0]) | (static_cast<uint16_t>(payload[1]) << 8);
        uint16_t hue = static_cast<uint16_t>(payload[2]) | (static_cast<uint16_t>(payload[3]) << 8);
        uint16_t sat = static_cast<uint16_t>(payload[4]) | (static_cast<uint16_t>(payload[5]) << 8);
        for (auto *lgt : this->bound_lights_) {
          if (lgt != nullptr) {
            auto call = lgt->make_call();
            call.set_brightness(static_cast<float>(lightness) / 65535.0f);
            call.set_state(lightness > 0);
            call.perform();
          }
        }
        if (opcode == OPCODE_LIGHT_HSL_SET) {
          uint8_t status_payload[6] = {payload[0], payload[1], payload[2], payload[3], payload[4], payload[5]};
          this->send_mesh_pdu(src, this->app_key_index_, OPCODE_LIGHT_HSL_STATUS, status_payload, sizeof(status_payload));
        }
      }
      break;
    case OPCODE_LIGHT_LIGHTNESS_GET: {
      uint16_t lightness = static_cast<uint16_t>(this->generic_level_state_ * 2);
      uint8_t status_payload[2] = {static_cast<uint8_t>(lightness & 0xFF),
                                   static_cast<uint8_t>((lightness >> 8) & 0xFF)};
      this->send_mesh_pdu(src, this->app_key_index_, OPCODE_LIGHT_LIGHTNESS_STATUS, status_payload, sizeof(status_payload));
      break;
    }
    case OPCODE_LIGHT_LIGHTNESS_SET:
      if (len >= 2) {
        uint16_t lightness = static_cast<uint16_t>(payload[0]) | (static_cast<uint16_t>(payload[1]) << 8);
        int16_t level = static_cast<int16_t>(lightness / 2);
        this->on_generic_level_set(src, dst, level, false);
        uint8_t status_payload[2] = {static_cast<uint8_t>(lightness & 0xFF),
                                     static_cast<uint8_t>((lightness >> 8) & 0xFF)};
        this->send_mesh_pdu(src, this->app_key_index_, OPCODE_LIGHT_LIGHTNESS_STATUS, status_payload, sizeof(status_payload));
      }
      break;
    case OPCODE_LIGHT_LIGHTNESS_SET_UNACK:
      if (len >= 2) {
        uint16_t lightness = static_cast<uint16_t>(payload[0]) | (static_cast<uint16_t>(payload[1]) << 8);
        int16_t level = static_cast<int16_t>(lightness / 2);
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

  uint8_t access_pdu[16] = {0};
  size_t access_len = 0;
  if (opcode > 0xFF) {
    access_pdu[access_len++] = (opcode >> 8) & 0xFF;
    access_pdu[access_len++] = opcode & 0xFF;
  } else {
    access_pdu[access_len++] = opcode & 0xFF;
  }
  if (payload != nullptr && len > 0 && access_len + len <= sizeof(access_pdu)) {
    std::memcpy(access_pdu + access_len, payload, len);
    access_len += len;
  }

  uint8_t app_nonce[13] = {0};
  app_nonce[0] = 0x01;  // Application Nonce
  app_nonce[1] = 0x00;  // ASZMIC = 0
  app_nonce[2] = (seq >> 16) & 0xFF;
  app_nonce[3] = (seq >> 8) & 0xFF;
  app_nonce[4] = seq & 0xFF;
  app_nonce[5] = (this->unicast_address_ >> 8) & 0xFF;
  app_nonce[6] = this->unicast_address_ & 0xFF;
  app_nonce[7] = (dst >> 8) & 0xFF;
  app_nonce[8] = dst & 0xFF;
  app_nonce[9] = (this->iv_index_ >> 24) & 0xFF;
  app_nonce[10] = (this->iv_index_ >> 16) & 0xFF;
  app_nonce[11] = (this->iv_index_ >> 8) & 0xFF;
  app_nonce[12] = this->iv_index_ & 0xFF;

  uint8_t upper_transport_pdu[24] = {0};
  size_t trans_mic_len = 4;
  if (this->app_key_.is_set) {
    encrypt_mesh_payload(this->app_key_.bytes.data(), app_nonce, access_pdu, access_len, upper_transport_pdu,
                         trans_mic_len);
  } else {
    std::memcpy(upper_transport_pdu, access_pdu, access_len);
  }
  size_t upper_transport_len = this->app_key_.is_set ? (access_len + trans_mic_len) : access_len;

  uint8_t lower_transport_pdu[25] = {0};
  lower_transport_pdu[0] = 0x40 | (this->aid_ & 0x3F);  // SEG=0, AKF=1, AID
  std::memcpy(lower_transport_pdu + 1, upper_transport_pdu, upper_transport_len);
  size_t lower_transport_len = 1 + upper_transport_len;

  uint8_t pdu_buffer[31] = {0};
  pdu_buffer[0] = this->nid_ & 0x7F;
  pdu_buffer[1] = 0x07;  // TTL 7
  pdu_buffer[2] = (seq >> 16) & 0xFF;
  pdu_buffer[3] = (seq >> 8) & 0xFF;
  pdu_buffer[4] = seq & 0xFF;
  pdu_buffer[5] = (this->unicast_address_ >> 8) & 0xFF;
  pdu_buffer[6] = this->unicast_address_ & 0xFF;

  uint8_t net_plaintext[26] = {0};
  net_plaintext[0] = (dst >> 8) & 0xFF;
  net_plaintext[1] = dst & 0xFF;
  std::memcpy(net_plaintext + 2, lower_transport_pdu, lower_transport_len);
  size_t net_plaintext_len = 2 + lower_transport_len;

  size_t net_mic_len = 4;
  uint8_t net_nonce[13] = {0};
  net_nonce[0] = 0x00;  // Network Nonce
  net_nonce[1] = 0x07;  // TTL 7
  net_nonce[2] = (seq >> 16) & 0xFF;
  net_nonce[3] = (seq >> 8) & 0xFF;
  net_nonce[4] = seq & 0xFF;
  net_nonce[5] = (this->unicast_address_ >> 8) & 0xFF;
  net_nonce[6] = this->unicast_address_ & 0xFF;
  net_nonce[7] = 0x00;
  net_nonce[8] = 0x00;
  net_nonce[9] = (this->iv_index_ >> 24) & 0xFF;
  net_nonce[10] = (this->iv_index_ >> 16) & 0xFF;
  net_nonce[11] = (this->iv_index_ >> 8) & 0xFF;
  net_nonce[12] = this->iv_index_ & 0xFF;

  if (this->net_key_.is_set) {
    encrypt_mesh_payload(this->encryption_key_, net_nonce, net_plaintext, net_plaintext_len, pdu_buffer + 7,
                         net_mic_len);
    size_t encrypted_frame_len = 7 + net_plaintext_len + net_mic_len;

    uint8_t header_to_obfuscate[6] = {pdu_buffer[1], pdu_buffer[2], pdu_buffer[3],
                                      pdu_buffer[4], pdu_buffer[5], pdu_buffer[6]};
    obfuscate_header(this->privacy_key_, this->iv_index_, pdu_buffer + 7, header_to_obfuscate);
    std::memcpy(pdu_buffer + 1, header_to_obfuscate, 6);

    std::memcpy(this->last_outgoing_frame_.data(), pdu_buffer, encrypted_frame_len);
    this->last_outgoing_frame_len_ = encrypted_frame_len;
  } else {
    std::memcpy(pdu_buffer + 7, net_plaintext, net_plaintext_len);
    size_t plain_frame_len = 7 + net_plaintext_len;
    std::memcpy(this->last_outgoing_frame_.data(), pdu_buffer, plain_frame_len);
    this->last_outgoing_frame_len_ = plain_frame_len;
  }

  if (this->seq_number_ % 10 == 0) {
    this->pref_.save(&this->seq_number_);
  }

  this->send_proxy_data_out_notification(pdu_buffer, this->last_outgoing_frame_len_);
}

void BluetoothSIGMesh::handle_proxy_pdu(const uint8_t *data, size_t len) {
  if (len < 1 || data == nullptr) {
    return;
  }
  uint8_t sar = (data[0] >> 6) & 0x03;
  uint8_t pdu_type = data[0] & 0x3F;

  if (sar == 0x00) {  // Complete PDU
    this->proxy_sar_len_ = 0;
  } else if (sar == 0x01) {  // First Segment
    this->proxy_sar_len_ = std::min(len - 1, this->proxy_sar_buffer_.size());
    std::memcpy(this->proxy_sar_buffer_.data(), data + 1, this->proxy_sar_len_);
    return;
  } else if (sar == 0x02) {  // Continuation Segment
    size_t chunk = std::min(len - 1, this->proxy_sar_buffer_.size() - this->proxy_sar_len_);
    std::memcpy(this->proxy_sar_buffer_.data() + this->proxy_sar_len_, data + 1, chunk);
    this->proxy_sar_len_ += chunk;
    return;
  } else if (sar == 0x03) {  // Last Segment
    size_t chunk = std::min(len - 1, this->proxy_sar_buffer_.size() - this->proxy_sar_len_);
    std::memcpy(this->proxy_sar_buffer_.data() + this->proxy_sar_len_, data + 1, chunk);
    this->proxy_sar_len_ += chunk;
    const uint8_t *complete_data = this->proxy_sar_buffer_.data();
    size_t complete_len = this->proxy_sar_len_;
    if (complete_len > 0) {
      if (pdu_type == PROXY_PDU_TYPE_NET_PDU) {
        this->process_mesh_pdu(complete_data, complete_len);
      }
    }
    this->proxy_sar_len_ = 0;
    return;
  }

  const uint8_t *payload = (sar == 0x00) ? (data + 1) : this->proxy_sar_buffer_.data();
  size_t payload_len = (sar == 0x00) ? (len - 1) : this->proxy_sar_len_;

  switch (pdu_type) {
    case PROXY_PDU_TYPE_NET_PDU:
      this->process_mesh_pdu(payload, payload_len);
      break;
    case PROXY_PDU_TYPE_CONFIG:
      if (payload_len >= 1) {
        uint8_t proxy_opcode = payload[0];
        if (proxy_opcode == PROXY_CONFIG_OPCODE_SET_FILTER_TYPE && payload_len >= 2) {
          this->set_proxy_filter_type(payload[1]);
        }
      }
      break;
    default:
      ESP_LOGVV(TAG, "Handled Proxy PDU type %u, len %zu", pdu_type, payload_len);
      break;
  }
}

void BluetoothSIGMesh::send_proxy_data_out_notification(const uint8_t *data, size_t len) {
  if (data == nullptr || len == 0) {
    return;
  }
  ESP_LOGVV(TAG, "Notifying GATT Proxy Data Out (0x2ADF), len: %zu", len);
  if (this->proxy_data_out_callback_ != nullptr) {
    this->proxy_data_out_callback_(data, len);
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
