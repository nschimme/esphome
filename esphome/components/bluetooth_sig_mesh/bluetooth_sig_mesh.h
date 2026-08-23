#pragma once

#ifdef USE_BLUETOOTH_SIG_MESH

#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace esphome {
namespace bluetooth_sig_mesh {

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

  bool is_node_enabled() const { return this->enable_node_; }
  bool is_proxy_enabled() const { return this->enable_proxy_; }
  bool is_provisioned() const { return this->provision_state_ == ProvisioningState::PROVISIONED; }
  uint16_t get_unicast_address() const { return this->unicast_address_; }
  ProvisioningState get_provisioning_state() const { return this->provision_state_; }

  // Mesh PDU and Packet Processing Primitives
  virtual void process_mesh_pdu(const uint8_t *data, size_t len);
  virtual void send_mesh_pdu(uint16_t dst, uint16_t app_idx, const uint8_t *payload, size_t len);
  virtual void on_generic_onoff_get(uint16_t src, uint16_t dst);
  virtual void on_generic_onoff_set(uint16_t src, uint16_t dst, bool state, bool ack);

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
};

extern BluetoothSIGMesh *global_bluetooth_sig_mesh;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

}  // namespace bluetooth_sig_mesh
}  // namespace esphome

#endif  // USE_BLUETOOTH_SIG_MESH
