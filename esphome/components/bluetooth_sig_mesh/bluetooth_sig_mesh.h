#pragma once

#ifdef USE_BLUETOOTH_SIG_MESH

#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include <string>
#include <vector>

namespace esphome {
namespace bluetooth_sig_mesh {

enum class MeshRole : uint8_t {
  NODE = 0,
  PROXY = 1,
  BOTH = 2,
};

class BluetoothSIGMesh : public Component {
 public:
  BluetoothSIGMesh() = default;

  void setup() override;
  void loop() override;
  void dump_config() override;

  void set_enable_node(bool enable_node) { this->enable_node_ = enable_node; }
  void set_enable_proxy(bool enable_proxy) { this->enable_proxy_ = enable_proxy; }
  void set_net_key(const std::string &net_key) { this->net_key_ = net_key; }
  void set_app_key(const std::string &app_key) { this->app_key_ = app_key; }
  void set_unicast_address(uint16_t address) { this->unicast_address_ = address; }

  bool is_node_enabled() const { return this->enable_node_; }
  bool is_proxy_enabled() const { return this->enable_proxy_; }
  uint16_t get_unicast_address() const { return this->unicast_address_; }

 protected:
  bool enable_node_{true};
  bool enable_proxy_{true};
  std::string net_key_{};
  std::string app_key_{};
  uint16_t unicast_address_{0x0001};
  bool is_provisioned_{false};
};

extern BluetoothSIGMesh *global_bluetooth_sig_mesh;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

}  // namespace bluetooth_sig_mesh
}  // namespace esphome

#endif  // USE_BLUETOOTH_SIG_MESH
