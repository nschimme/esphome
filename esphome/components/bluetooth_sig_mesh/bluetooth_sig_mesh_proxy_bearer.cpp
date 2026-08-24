#include "bluetooth_sig_mesh_proxy_bearer.h"

#ifdef USE_BLUETOOTH_SIG_MESH

#include "bluetooth_sig_mesh.h"
#include "esphome/core/log.h"
#include <algorithm>
#include <cstring>

namespace esphome {
namespace bluetooth_sig_mesh {

static const char *const TAG = "bluetooth_sig_mesh.proxy";

void BluetoothSIGMeshProxyBearer::on_proxy_data_in_write(const uint8_t *data, size_t len) {
  ESP_LOGD(TAG, "GATT Proxy Data In (0x2ADE) write received, len: %zu", len);
  this->handle_proxy_pdu(data, len);
}

void BluetoothSIGMeshProxyBearer::handle_proxy_pdu(const uint8_t *data, size_t len) {
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
    if (complete_len > 0 && this->mesh_ != nullptr) {
      if (pdu_type == PROXY_PDU_TYPE_NET_PDU) {
        this->mesh_->process_mesh_pdu(complete_data, complete_len);
      }
    }
    this->proxy_sar_len_ = 0;
    return;
  }

  const uint8_t *payload = (sar == 0x00) ? (data + 1) : this->proxy_sar_buffer_.data();
  size_t payload_len = (sar == 0x00) ? (len - 1) : this->proxy_sar_len_;

  switch (pdu_type) {
    case PROXY_PDU_TYPE_NET_PDU:
      if (this->mesh_ != nullptr) {
        this->mesh_->process_mesh_pdu(payload, payload_len);
      }
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

void BluetoothSIGMeshProxyBearer::send_proxy_data_out_notification(const uint8_t *data, size_t len) {
  if (data == nullptr || len == 0) {
    return;
  }
  ESP_LOGVV(TAG, "Notifying GATT Proxy Data Out (0x2ADF), len: %zu", len);
  if (this->proxy_data_out_callback_ != nullptr) {
    this->proxy_data_out_callback_(data, len);
  }
}

void BluetoothSIGMeshProxyBearer::set_proxy_filter_type(uint8_t filter_type) {
  this->proxy_filter_type_ = filter_type;
  this->proxy_filter_addresses_.clear();
  ESP_LOGI(TAG, "Proxy Filter type set to %s", filter_type == 0x00 ? "White List" : "Black List");
}

void BluetoothSIGMeshProxyBearer::add_proxy_filter_address(uint16_t address) {
  this->proxy_filter_addresses_.insert(address);
  ESP_LOGD(TAG, "Added 0x%04X to Proxy Filter", address);
}

void BluetoothSIGMeshProxyBearer::remove_proxy_filter_address(uint16_t address) {
  this->proxy_filter_addresses_.erase(address);
  ESP_LOGD(TAG, "Removed 0x%04X from Proxy Filter", address);
}

}  // namespace bluetooth_sig_mesh
}  // namespace esphome

#endif  // USE_BLUETOOTH_SIG_MESH
