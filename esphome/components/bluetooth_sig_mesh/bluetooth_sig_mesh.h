#pragma once

#ifdef USE_BLUETOOTH_SIG_MESH

#include "esphome/components/light/light_state.h"
#include "esphome/components/switch/switch.h"
#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
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

// Bluetooth SIG Mesh Key Sizes
constexpr size_t MESH_KEY_SIZE = 16;
constexpr size_t MESH_UUID_SIZE = 16;

struct MeshKey {
  std::array<uint8_t, MESH_KEY_SIZE> bytes{};
  bool is_set{false};
};

enum class MeshRole : uint8_t {
  NODE = 0,
  PROXY = 1,
  BOTH = 2,
};

enum class ProvisioningState : uint8_t {
  UNPROVISIONED = 0,
  PROVISIONING = 1,
  PROVISIONED = 2,
  FAILED = 3,
};

struct SIGMeshElement {
  uint16_t loc{0x0000};
  uint8_t num_s_models{0};
  uint8_t num_v_models{0};
  std::vector<uint16_t> sig_models{};
};

struct MeshNetworkPDUHeader {
  uint8_t nid{0};
  bool ctl{false};
  uint8_t ttl{0};
  uint32_t seq{0};
  uint16_t src{0};
  uint16_t dst{0};
};

class BluetoothSIGMesh : public Component {
 public:
  BluetoothSIGMesh() = default;

  void setup() override;
  void loop() override;
  void dump_config() override;

  void set_enable_node(bool enable_node) { this->enable_node_ = enable_node; }
  void set_enable_proxy(bool enable_proxy) { this->enable_proxy_ = enable_proxy; }
  void set_net_key(const std::string &net_key_hex);
  void set_app_key(const std::string &app_key_hex);
  void set_unicast_address(uint16_t address) { this->unicast_address_ = address; }
  void add_bound_switch(switch_::Switch *sw) { this->bound_switches_.push_back(sw); }
  void add_bound_light(light::LightState *lgt) { this->bound_lights_.push_back(lgt); }

  bool is_node_enabled() const { return this->enable_node_; }
  bool is_proxy_enabled() const { return this->enable_proxy_; }
  bool is_provisioned() const { return this->provision_state_ == ProvisioningState::PROVISIONED; }
  uint16_t get_unicast_address() const { return this->unicast_address_; }
  ProvisioningState get_provisioning_state() const { return this->provision_state_; }

  // Mesh Network and Transport Layer Processing
  virtual void process_mesh_pdu(const uint8_t *data, size_t len);
  virtual void process_network_pdu(const MeshNetworkPDUHeader &hdr, const uint8_t *payload, size_t len);
  virtual void process_access_pdu(uint16_t src, uint16_t dst, uint16_t opcode, const uint8_t *payload, size_t len);
  virtual void send_mesh_pdu(uint16_t dst, uint16_t app_idx, uint16_t opcode, const uint8_t *payload, size_t len);

  // GATT Proxy Bearer & Proxy Filter Management
  virtual void handle_proxy_pdu(const uint8_t *data, size_t len);
  virtual void set_proxy_filter_type(uint8_t filter_type);
  virtual void add_proxy_filter_address(uint16_t address);
  virtual void remove_proxy_filter_address(uint16_t address);

  // Model Event Handlers
  virtual void on_generic_onoff_get(uint16_t src, uint16_t dst);
  virtual void on_generic_onoff_set(uint16_t src, uint16_t dst, bool state, bool ack);
  virtual void on_generic_level_get(uint16_t src, uint16_t dst);
  virtual void on_generic_level_set(uint16_t src, uint16_t dst, int16_t level, bool ack);

 protected:
  bool parse_hex_key_(const std::string &hex, MeshKey &out_key);

  bool enable_node_{true};
  bool enable_proxy_{true};
  MeshKey net_key_{};
  MeshKey app_key_{};
  uint16_t unicast_address_{0x0001};
  uint16_t net_key_index_{0x0000};
  uint16_t app_key_index_{0x0000};
  uint32_t iv_index_{0x00000000};
  uint32_t seq_number_{0x000001};
  ProvisioningState provision_state_{ProvisioningState::UNPROVISIONED};
  std::vector<SIGMeshElement> elements_{};
  bool generic_onoff_state_{false};
  int16_t generic_level_state_{0};

  uint8_t proxy_filter_type_{PROXY_FILTER_TYPE_WHITE_LIST};
  std::set<uint16_t> proxy_filter_addresses_{};

  std::vector<switch_::Switch *> bound_switches_{};
  std::vector<light::LightState *> bound_lights_{};
};

extern BluetoothSIGMesh *global_bluetooth_sig_mesh;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

}  // namespace bluetooth_sig_mesh
}  // namespace esphome

#endif  // USE_BLUETOOTH_SIG_MESH
