#include "bluetooth_sig_mesh.h"

#ifdef USE_BLUETOOTH_SIG_MESH

#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace esphome {
namespace bluetooth_sig_mesh {

static const char *const TAG = "bluetooth_sig_mesh";

BluetoothSIGMesh *global_bluetooth_sig_mesh = nullptr;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

void BluetoothSIGMesh::derive_app_keys_() {
  if (this->app_key_.is_set) {
    this->aid_ = BluetoothSIGMeshCrypto::mesh_k4(this->app_key_.bytes.data());
    ESP_LOGI(TAG, "Derived AppKey parameters: AID=0x%02X", this->aid_);
  }
}

void BluetoothSIGMesh::derive_net_keys_() {
  if (this->net_key_.is_set) {
    static const uint8_t p[1] = {0x00};
    BluetoothSIGMeshCrypto::mesh_k2(this->net_key_.bytes.data(), p, 1, &this->nid_, this->encryption_key_,
                                    this->privacy_key_);
    ESP_LOGI(TAG, "Derived NetKey parameters: NID=0x%02X", this->nid_);
  }
}

void BluetoothSIGMesh::set_net_key(const std::string &net_key_hex) {
  if (BluetoothSIGMeshCrypto::parse_hex_key(net_key_hex, this->net_key_)) {
    ESP_LOGI(TAG, "Network key configured successfully");
    this->derive_net_keys_();
    if (this->app_key_.is_set) {
      this->provision_state_ = ProvisioningState::PROVISIONED;
    }
  }
}

void BluetoothSIGMesh::set_app_key(const std::string &app_key_hex) {
  if (BluetoothSIGMeshCrypto::parse_hex_key(app_key_hex, this->app_key_)) {
    ESP_LOGI(TAG, "Application key configured successfully");
    this->derive_app_keys_();
    if (this->net_key_.is_set) {
      this->provision_state_ = ProvisioningState::PROVISIONED;
    }
  }
}

void BluetoothSIGMesh::add_remote_node(uint16_t address, const std::string &device_key_hex, const std::string &name) {
  MeshKey dev_key{};
  if (BluetoothSIGMeshCrypto::parse_hex_key(device_key_hex, dev_key)) {
    ESP_LOGI(TAG, "Registered remote mesh device 0x%04X ('%s')", address, name.c_str());
  }
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

  for (const auto &md : device.get_mesh_datas()) {
    if (md.uuid == ble_device_base::ESPBTUUID::from_uint16(MESH_AD_TYPE_MESSAGE)) {  // 0x2A Mesh Message
      ESP_LOGVV(TAG, "Received Mesh Message AD Type 0x2A from MAC %012" PRIX64, device.address_uint64());
      this->process_mesh_pdu(md.data.data(), md.data.size());
      return true;
    }
    if (md.uuid == ble_device_base::ESPBTUUID::from_uint16(MESH_AD_TYPE_BEACON)) {  // 0x2B Mesh Beacon
      ESP_LOGI(TAG, "Unprovisioned Mesh Beacon (0x2B) discovered from MAC %012" PRIX64, device.address_uint64());
      return true;
    }
    if (md.uuid == ble_device_base::ESPBTUUID::from_uint16(MESH_AD_TYPE_PB_ADV)) {  // 0x29 PB-ADV
      ESP_LOGI(TAG, "Unprovisioned PB-ADV (0x29) advertisement discovered from MAC %012" PRIX64,
               device.address_uint64());
      return true;
    }
  }

  return false;
}

void BluetoothSIGMesh::setup() {
  global_bluetooth_sig_mesh = this;
  ESP_LOGCONFIG(TAG, "Setting up Bluetooth SIG Mesh...");
  this->proxy_bearer_.set_mesh(this);
  this->dfu_server_.set_mesh(this);
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
    BluetoothSIGMeshCrypto::obfuscate_header(this->privacy_key_, this->iv_index_, data + 7, header_copy);
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

    if (BluetoothSIGMeshCrypto::decrypt_mesh_payload(this->encryption_key_, nonce, encrypted_pdu, encrypted_len,
                                                      decrypted, mic_len)) {
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

        BluetoothSIGMeshCrypto::obfuscate_header(this->privacy_key_, this->iv_index_, data + 7, relay_hdr);
        std::memcpy(retransmitted_pdu + 1, relay_hdr, 6);

        ESP_LOGD(TAG, "Relaying Mesh PDU from 0x%04X to 0x%04X (Decremented TTL: %u)", hdr.src, hdr.dst, hdr.ttl - 1);
        std::memcpy(this->last_outgoing_frame_.data(), retransmitted_pdu, len);
        this->last_outgoing_frame_len_ = len;
        this->send_proxy_data_out_notification(retransmitted_pdu, len);
        this->transmit_last_outgoing_frame();
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
    if (!BluetoothSIGMeshCrypto::decrypt_mesh_payload(this->app_key_.bytes.data(), app_nonce, upper_transport_pdu,
                                                      upper_transport_len, access_pdu, mic_len)) {
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
    case OPCODE_SENSOR_GET: {
      if (!this->bound_sensors_.empty()) {
        for (const auto &bs : this->bound_sensors_) {
          if (bs.sensor != nullptr && bs.sensor->has_state()) {
            int16_t val = static_cast<int16_t>(bs.sensor->state * 100.0f);
            uint8_t status_payload[4] = {
                static_cast<uint8_t>(bs.property_id & 0xFF),
                static_cast<uint8_t>((bs.property_id >> 8) & 0xFF),
                static_cast<uint8_t>(val & 0xFF),
                static_cast<uint8_t>((val >> 8) & 0xFF),
            };
            this->send_mesh_pdu(src, this->app_key_index_, OPCODE_SENSOR_STATUS, status_payload, sizeof(status_payload));
          }
        }
      }
      break;
    }
    default:
      if (!this->dfu_server_.handle_dfu_opcode(src, opcode, payload, len)) {
        ESP_LOGD(TAG, "Access PDU from 0x%04X, Opcode: 0x%04X, Len: %zu", src, opcode, len);
      }
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
    BluetoothSIGMeshCrypto::encrypt_mesh_payload(this->app_key_.bytes.data(), app_nonce, access_pdu, access_len,
                                                  upper_transport_pdu, trans_mic_len);
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
    BluetoothSIGMeshCrypto::encrypt_mesh_payload(this->encryption_key_, net_nonce, net_plaintext, net_plaintext_len,
                                                  pdu_buffer + 7, net_mic_len);
    size_t encrypted_frame_len = 7 + net_plaintext_len + net_mic_len;

    uint8_t header_to_obfuscate[6] = {pdu_buffer[1], pdu_buffer[2], pdu_buffer[3],
                                      pdu_buffer[4], pdu_buffer[5], pdu_buffer[6]};
    BluetoothSIGMeshCrypto::obfuscate_header(this->privacy_key_, this->iv_index_, pdu_buffer + 7,
                                              header_to_obfuscate);
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
