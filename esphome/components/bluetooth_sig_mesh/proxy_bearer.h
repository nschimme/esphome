#pragma once

#ifdef USE_BLUETOOTH_SIG_MESH

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <set>

namespace esphome {
namespace bluetooth_sig_mesh {

class BluetoothSIGMesh;

class BluetoothSIGMeshProxyBearer {
 public:
  BluetoothSIGMeshProxyBearer() = default;

  void set_mesh(BluetoothSIGMesh *mesh) { this->mesh_ = mesh; }

  using ProxyDataOutCallback = std::function<void(const uint8_t *data, size_t len)>;
  void set_proxy_data_out_callback(ProxyDataOutCallback &&cb) { this->proxy_data_out_callback_ = std::move(cb); }

  void handle_proxy_pdu(const uint8_t *data, size_t len);
  void send_proxy_data_out_notification(const uint8_t *data, size_t len);
  void set_proxy_filter_type(uint8_t filter_type);
  void add_proxy_filter_address(uint16_t address);
  void remove_proxy_filter_address(uint16_t address);
  void on_proxy_data_in_write(const uint8_t *data, size_t len);

 protected:
  BluetoothSIGMesh *mesh_{nullptr};
  uint8_t proxy_filter_type_{0x00}; // White List
  std::set<uint16_t> proxy_filter_addresses_{};
  ProxyDataOutCallback proxy_data_out_callback_{nullptr};

  std::array<uint8_t, 64> proxy_sar_buffer_{};
  size_t proxy_sar_len_{0};
};

}  // namespace bluetooth_sig_mesh
}  // namespace esphome

#endif  // USE_BLUETOOTH_SIG_MESH
