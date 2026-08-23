#pragma once

#ifdef USE_BLUETOOTH_SIG_MESH

#include "esphome/components/ble_device_base/ble_aes_ccm.h"
#include "esphome/components/ble_device_base/ble_device.h"
#include "esphome/components/light/light_state.h"
#include "esphome/components/switch/switch.h"
#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include "esphome/core/preferences.h"
#include <array>
#include <cstdint>
#include <set>
#include <string>
#include <vector>

namespace esphome {
namespace bluetooth_sig_mesh {

// Bluetooth SIG Mesh AD Types
constexpr uint8_t MESH_AD_TYPE_PB_ADV = 0x29;
constexpr uint8_t MESH_AD_TYPE_MESSAGE = 0x2A;
constexpr uint8_t MESH_AD_TYPE_BEACON = 0x2B;

// Bluetooth SIG Mesh Service UUIDs (16-bit)
constexpr uint16_t MESH_PROVISIONING_SERVICE_UUID = 0x1827;
constexpr uint16_t MESH_PROXY_SERVICE_UUID = 0x1828;

// Bluetooth SIG Mesh Characteristic UUIDs (16-bit)
constexpr uint16_t MESH_PROVISIONING_DATA_IN_UUID = 0x2ADB;
constexpr uint16_t MESH_PROVISIONING_DATA_OUT_UUID = 0x2ADC;
constexpr uint16_t MESH_PROXY_DATA_IN_UUID = 0x2ADE;
constexpr uint16_t MESH_PROXY_DATA_OUT_UUID = 0x2ADF;

// Mesh Proxy PDU Types
constexpr uint8_t PROXY_PDU_TYPE_NET_PDU = 0x00;
constexpr uint8_t PROXY_PDU_TYPE_BEACON = 0x01;
constexpr uint8_t PROXY_PDU_TYPE_CONFIG = 0x02;
constexpr uint8_t PROXY_PDU_TYPE_PROVISIONING = 0x03;

// Proxy Configuration Opcodes
constexpr uint8_t PROXY_CONFIG_OPCODE_SET_FILTER_TYPE = 0x00;
constexpr uint8_t PROXY_CONFIG_OPCODE_ADD_ADDRESSES = 0x01;
constexpr uint8_t PROXY_CONFIG_OPCODE_REMOVE_ADDRESSES = 0x02;
constexpr uint8_t PROXY_CONFIG_OPCODE_FILTER_STATUS = 0x03;

// Proxy Filter Types
constexpr uint8_t PROXY_FILTER_TYPE_WHITE_LIST = 0x00;
constexpr uint8_t PROXY_FILTER_TYPE_BLACK_LIST = 0x01;

// Mesh Model IDs (SIG Defined 16-bit)
constexpr uint16_t MESH_MODEL_ID_CONFIG_SERVER = 0x0000;
constexpr uint16_t MESH_MODEL_ID_CONFIG_CLIENT = 0x0001;
constexpr uint16_t MESH_MODEL_ID_HEALTH_SERVER = 0x0002;
constexpr uint16_t MESH_MODEL_ID_GENERIC_ONOFF_SERVER = 0x1000;
constexpr uint16_t MESH_MODEL_ID_GENERIC_ONOFF_CLIENT = 0x1001;
constexpr uint16_t MESH_MODEL_ID_GENERIC_LEVEL_SERVER = 0x1002;
constexpr uint16_t MESH_MODEL_ID_GENERIC_LEVEL_CLIENT = 0x1003;

// SIG Mesh Opcodes
constexpr uint16_t OPCODE_CONFIG_APPKEY_ADD = 0x0000;
constexpr uint16_t OPCODE_CONFIG_APPKEY_STATUS = 0x8003;
constexpr uint16_t OPCODE_CONFIG_MODEL_APP_BIND = 0x803D;
constexpr uint16_t OPCODE_CONFIG_MODEL_APP_STATUS = 0x803E;
constexpr uint16_t OPCODE_GENERIC_ONOFF_GET = 0x8201;
constexpr uint16_t OPCODE_GENERIC_ONOFF_SET = 0x8202;
constexpr uint16_t OPCODE_GENERIC_ONOFF_SET_UNACK = 0x8203;
constexpr uint16_t OPCODE_GENERIC_ONOFF_STATUS = 0x8204;
constexpr uint16_t OPCODE_GENERIC_LEVEL_GET = 0x8205;
constexpr uint16_t OPCODE_GENERIC_LEVEL_SET = 0x8206;
constexpr uint16_t OPCODE_GENERIC_LEVEL_SET_UNACK = 0x8207;
constexpr uint16_t OPCODE_GENERIC_LEVEL_STATUS = 0x8208;

// Light Lightness Opcodes
constexpr uint16_t OPCODE_LIGHT_LIGHTNESS_GET = 0x824B;
constexpr uint16_t OPCODE_LIGHT_LIGHTNESS_SET = 0x824C;
constexpr uint16_t OPCODE_LIGHT_LIGHTNESS_SET_UNACK = 0x824D;
constexpr uint16_t OPCODE_LIGHT_LIGHTNESS_STATUS = 0x824E;

// Bluetooth SIG Mesh Key Sizes
constexpr size_t MESH_KEY_SIZE = 16;
constexpr size_t MESH_UUID_SIZE = 16;

struct MeshKey {
  std::array<uint8_t, MESH_KEY_SIZE> bytes{};
  bool is_set{false};
};

enum class ProvisioningState : uint8_t {
  UNPROVISIONED = 0,
  PROVISIONING = 1,
  PROVISIONED = 2,
  FAILED = 3,
};

struct MeshNetworkPDUHeader {
  uint8_t nid{0};
  bool ctl{false};
  uint8_t ttl{0};
  uint32_t seq{0};
  uint16_t src{0};
  uint16_t dst{0};
};

class BluetoothSIGMesh : public Component, public ble_device_base::ESPBTDeviceListener {
 public:
  BluetoothSIGMesh() = default;

  void setup() override;
  void loop() override;
  void dump_config() override;

  bool parse_device(const ble_device_base::ESPBTDevice &device) override;

  void set_relay(bool relay) { this->relay_enabled_ = relay; }
  void set_advertise_unprovisioned(bool advertise) { this->advertise_unprovisioned_ = advertise; }
  void set_net_key(const std::string &net_key_hex);
  void set_app_key(const std::string &app_key_hex);
  void set_unicast_address(uint16_t address) { this->unicast_address_ = address; }
  void add_bound_switch(switch_::Switch *sw) { this->bound_switches_.push_back(sw); }
  void add_bound_light(light::LightState *lgt) { this->bound_lights_.push_back(lgt); }
  void add_remote_node(uint16_t address, const std::string &device_key_hex, const std::string &name = "");

  bool is_relay_enabled() const { return this->relay_enabled_; }
  bool is_provisioned() const { return this->provision_state_ == ProvisioningState::PROVISIONED; }
  uint16_t get_unicast_address() const { return this->unicast_address_; }
  ProvisioningState get_provisioning_state() const { return this->provision_state_; }

  // Cryptographic Helper Functions & Derivations
  static void mesh_aes_cmac(const uint8_t key[16], const uint8_t *msg, size_t len, uint8_t out[16]);
  static void mesh_s1(const uint8_t *m, size_t len, uint8_t out[16]);
  static void mesh_k1(const uint8_t n[16], const uint8_t *p, size_t p_len, uint8_t out[16]);
  static void mesh_k2(const uint8_t net_key[16], const uint8_t *p, size_t p_len, uint8_t *out_nid, uint8_t out_ek[16],
                      uint8_t out_pk[16]);
  static uint8_t mesh_k4(const uint8_t app_key[16]);
  static bool decrypt_mesh_payload(const uint8_t key[16], const uint8_t nonce[13], const uint8_t *ct, size_t ct_len,
                                   uint8_t *pt, size_t mic_len);
  static void encrypt_mesh_payload(const uint8_t key[16], const uint8_t nonce[13], const uint8_t *pt, size_t pt_len,
                                   uint8_t *ct, size_t mic_len);
  static void obfuscate_header(const uint8_t privacy_key[16], uint32_t iv_index, const uint8_t privacy_random[7],
                               uint8_t header_data[6]);

  // Mesh Network and Transport Layer Processing
  virtual void process_mesh_pdu(const uint8_t *data, size_t len);
  virtual void process_network_pdu(const MeshNetworkPDUHeader &hdr, const uint8_t *payload, size_t len);
  virtual void process_access_pdu(uint16_t src, uint16_t dst, uint16_t opcode, const uint8_t *payload, size_t len);
  virtual void send_mesh_pdu(uint16_t dst, uint16_t app_idx, uint16_t opcode, const uint8_t *payload, size_t len);

  // Client Command Helpers
  void send_onoff(uint16_t dst, bool state, bool ack = true);
  void send_level(uint16_t dst, int16_t level, bool ack = true);
  void send_lightness(uint16_t dst, uint16_t lightness, bool ack = true);

  // Event Listener Callbacks for Client Platforms
  using NodeSeenCallback = std::function<void(uint16_t src, uint16_t opcode, const uint8_t *payload, size_t len)>;
  void add_node_seen_callback(NodeSeenCallback &&cb) { this->node_seen_callbacks_.push_back(std::move(cb)); }

  // GATT Proxy Bearer & Proxy Filter Management
  using ProxyDataOutCallback = std::function<void(const uint8_t *data, size_t len)>;
  void set_proxy_data_out_callback(ProxyDataOutCallback &&cb) { this->proxy_data_out_callback_ = std::move(cb); }

  virtual void handle_proxy_pdu(const uint8_t *data, size_t len);
  virtual void send_proxy_data_out_notification(const uint8_t *data, size_t len);
  virtual void set_proxy_filter_type(uint8_t filter_type);
  virtual void add_proxy_filter_address(uint16_t address);
  virtual void remove_proxy_filter_address(uint16_t address);
  virtual void on_proxy_data_in_write(const uint8_t *data, size_t len);

  // Model Event Handlers
  virtual void on_generic_onoff_get(uint16_t src, uint16_t dst);
  virtual void on_generic_onoff_set(uint16_t src, uint16_t dst, bool state, bool ack);
  virtual void on_generic_level_get(uint16_t src, uint16_t dst);
  virtual void on_generic_level_set(uint16_t src, uint16_t dst, int16_t level, bool ack);

  const uint8_t *get_last_outgoing_frame_data() const { return this->last_outgoing_frame_.data(); }
  size_t get_last_outgoing_frame_len() const { return this->last_outgoing_frame_len_; }

 protected:
  bool parse_hex_key_(const std::string &hex, MeshKey &out_key);
  void derive_net_keys_();
  void derive_app_keys_();

  bool relay_enabled_{true};
  bool advertise_unprovisioned_{false};
  MeshKey net_key_{};
  MeshKey app_key_{};
  uint8_t nid_{0};
  uint8_t encryption_key_[16]{0};
  uint8_t privacy_key_[16]{0};
  uint8_t aid_{0};

  uint16_t unicast_address_{0x0001};
  uint16_t net_key_index_{0x0000};
  uint16_t app_key_index_{0x0000};
  uint32_t iv_index_{0x00000000};
  uint32_t seq_number_{0x000001};
  ProvisioningState provision_state_{ProvisioningState::UNPROVISIONED};
  bool generic_onoff_state_{false};
  int16_t generic_level_state_{0};

  uint8_t proxy_filter_type_{PROXY_FILTER_TYPE_WHITE_LIST};
  std::set<uint16_t> proxy_filter_addresses_{};

  std::vector<switch_::Switch *> bound_switches_{};
  std::vector<light::LightState *> bound_lights_{};
  std::vector<NodeSeenCallback> node_seen_callbacks_{};
  ProxyDataOutCallback proxy_data_out_callback_{nullptr};

  ESPPreferenceObject pref_{};

  std::array<uint8_t, 64> proxy_sar_buffer_{};
  size_t proxy_sar_len_{0};

  std::array<uint8_t, 31> last_outgoing_frame_{};
  size_t last_outgoing_frame_len_{0};
};

extern BluetoothSIGMesh *global_bluetooth_sig_mesh;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

}  // namespace bluetooth_sig_mesh
}  // namespace esphome

#endif  // USE_BLUETOOTH_SIG_MESH
