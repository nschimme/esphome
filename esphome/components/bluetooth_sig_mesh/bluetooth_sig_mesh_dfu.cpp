#include "bluetooth_sig_mesh_dfu.h"

#ifdef USE_BLUETOOTH_SIG_MESH

#include "bluetooth_sig_mesh.h"
#include "esphome/core/application.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"
#include <cstring>

namespace esphome {
namespace bluetooth_sig_mesh {

static const char *const TAG = "bluetooth_sig_mesh.dfu";

bool BluetoothSIGMeshDFUServer::handle_dfu_opcode(uint16_t src, uint16_t opcode, const uint8_t *payload, size_t len) {
  switch (opcode) {
    case OPCODE_FW_UPDATE_INFO_GET:
      this->handle_fw_info_get_(src);
      return true;
    case OPCODE_FW_UPDATE_START:
      this->handle_fw_update_start_(src, payload, len);
      return true;
    case OPCODE_FW_UPDATE_APPLY:
      this->handle_fw_update_apply_(src);
      return true;
    case OPCODE_BLOB_INFO_GET:
      this->handle_blob_info_get_(src);
      return true;
    case OPCODE_BLOB_TRANSFER_START:
      this->handle_blob_transfer_start_(src, payload, len);
      return true;
    case OPCODE_BLOB_BLOCK_START:
      this->handle_blob_block_start_(src, payload, len);
      return true;
    case OPCODE_BLOB_CHUNK_TRANSFER:
      this->handle_blob_chunk_transfer_(src, payload, len);
      return true;
    default:
      return false;
  }
}

void BluetoothSIGMeshDFUServer::handle_fw_info_get_(uint16_t src) {
  ESP_LOGI(TAG, "Firmware Update Info Get received from 0x%04X", src);
  uint8_t status_payload[8] = {
      0x01,        // Firmware list count = 1
      0x04,        // Image ID length = 4
      'E', 'S', 'P', 'H', // Image ID ("ESPH")
      0x01, 0x00   // Version 1.0
  };
  if (this->mesh_ != nullptr) {
    this->mesh_->send_mesh_pdu(src, 0x0000, OPCODE_FW_UPDATE_INFO_STATUS, status_payload, sizeof(status_payload));
  }
}

void BluetoothSIGMeshDFUServer::handle_fw_update_start_(uint16_t src, const uint8_t *payload, size_t len) {
  ESP_LOGI(TAG, "Firmware Update Start received from 0x%04X, len: %zu", src, len);
  if (len >= 4) {
    this->blob_size_ = (static_cast<uint32_t>(payload[0]) << 24) | (static_cast<uint32_t>(payload[1]) << 16) |
                       (static_cast<uint32_t>(payload[2]) << 8) | static_cast<uint32_t>(payload[3]);
    ESP_LOGI(TAG, "Initiating Mesh DFU for image size %" PRIu32 " bytes", this->blob_size_);

    this->ota_backend_ = ota::make_ota_backend();
    if (this->ota_backend_ != nullptr) {
      ota::OTAResponseTypes res = this->ota_backend_->begin(this->blob_size_);
      if (res == ota::OTA_RESPONSE_OK) {
        this->dfu_state_ = DFUState::TRANSFER_IN_PROGRESS;
        this->received_bytes_ = 0;
        ESP_LOGI(TAG, "OTA Backend initialized successfully for SIG Mesh DFU");
      } else {
        ESP_LOGE(TAG, "OTA Backend begin failed with code %d", res);
        this->dfu_state_ = DFUState::FAILED;
      }
    }
  }

  uint8_t status_payload[2] = {static_cast<uint8_t>(this->dfu_state_ == DFUState::FAILED ? 0x01 : 0x00), 0x00};
  if (this->mesh_ != nullptr) {
    this->mesh_->send_mesh_pdu(src, 0x0000, OPCODE_FW_UPDATE_STATUS, status_payload, sizeof(status_payload));
  }
}

void BluetoothSIGMeshDFUServer::handle_fw_update_apply_(uint16_t src) {
  ESP_LOGI(TAG, "Firmware Update Apply received from 0x%04X. Finalizing OTA and rebooting...", src);
  if (this->ota_backend_ != nullptr) {
    ota::OTAResponseTypes res = this->ota_backend_->end();
    if (res == ota::OTA_RESPONSE_OK) {
      ESP_LOGI(TAG, "SIG Mesh DFU firmware verification success! Safe rebooting...");
      uint8_t status_payload[2] = {0x00, 0x00};
      if (this->mesh_ != nullptr) {
        this->mesh_->send_mesh_pdu(src, 0x0000, OPCODE_FW_UPDATE_STATUS, status_payload, sizeof(status_payload));
      }
      App.safe_reboot();
      return;
    } else {
      ESP_LOGE(TAG, "SIG Mesh DFU end() failed with code %d", res);
    }
  }

  uint8_t status_payload[2] = {0x01, 0x00};
  if (this->mesh_ != nullptr) {
    this->mesh_->send_mesh_pdu(src, 0x0000, OPCODE_FW_UPDATE_STATUS, status_payload, sizeof(status_payload));
  }
}

void BluetoothSIGMeshDFUServer::handle_blob_info_get_(uint16_t src) {
  ESP_LOGI(TAG, "BLOB Information Get received from 0x%04X", src);
  uint8_t status_payload[8] = {
      0x0C, 0x00, // Min Block Size Log2 (4096 = 2^12)
      0x0C, 0x00, // Max Block Size Log2 (4096)
      0x20, 0x00, // Max Chunk Size (32 bytes)
      0x00, 0x00  // Capabilities
  };
  if (this->mesh_ != nullptr) {
    this->mesh_->send_mesh_pdu(src, 0x0000, OPCODE_BLOB_INFO_STATUS, status_payload, sizeof(status_payload));
  }
}

void BluetoothSIGMeshDFUServer::handle_blob_transfer_start_(uint16_t src, const uint8_t *payload, size_t len) {
  ESP_LOGI(TAG, "BLOB Transfer Start received from 0x%04X", src);
  if (len >= 12) {
    this->blob_id_ = (static_cast<uint64_t>(payload[0]) << 56) | (static_cast<uint64_t>(payload[1]) << 48) |
                     (static_cast<uint64_t>(payload[2]) << 40) | (static_cast<uint64_t>(payload[3]) << 32) |
                     (static_cast<uint64_t>(payload[4]) << 24) | (static_cast<uint64_t>(payload[5]) << 16) |
                     (static_cast<uint64_t>(payload[6]) << 8) | static_cast<uint64_t>(payload[7]);
    this->blob_size_ = (static_cast<uint32_t>(payload[8]) << 24) | (static_cast<uint32_t>(payload[9]) << 16) |
                       (static_cast<uint32_t>(payload[10]) << 8) | static_cast<uint32_t>(payload[11]);
    ESP_LOGI(TAG, "BLOB Transfer Start: ID=0x%" PRIx64 ", Size=%" PRIu32, this->blob_id_, this->blob_size_);
  }
  uint8_t status_payload[2] = {0x00, 0x00};
  if (this->mesh_ != nullptr) {
    this->mesh_->send_mesh_pdu(src, 0x0000, OPCODE_BLOB_TRANSFER_STATUS, status_payload, sizeof(status_payload));
  }
}

void BluetoothSIGMeshDFUServer::handle_blob_block_start_(uint16_t src, const uint8_t *payload, size_t len) {
  if (len >= 2) {
    this->current_block_num_ = decode_uint16_le(payload);
    ESP_LOGI(TAG, "BLOB Block Start received: Block #%" PRIu16, this->current_block_num_);
  }
  uint8_t status_payload[2] = {0x00, 0x00};
  if (this->mesh_ != nullptr) {
    this->mesh_->send_mesh_pdu(src, 0x0000, OPCODE_BLOB_BLOCK_STATUS, status_payload, sizeof(status_payload));
  }
}

void BluetoothSIGMeshDFUServer::handle_blob_chunk_transfer_(uint16_t src, const uint8_t *payload, size_t len) {
  if (len < 2 || payload == nullptr) {
    return;
  }
  uint16_t chunk_num = decode_uint16_le(payload);
  const uint8_t *chunk_data = payload + 2;
  size_t chunk_len = len - 2;

  if (this->ota_backend_ != nullptr && chunk_len > 0) {
    ota::OTAResponseTypes res = this->ota_backend_->write(const_cast<uint8_t *>(chunk_data), chunk_len);
    if (res == ota::OTA_RESPONSE_OK) {
      this->received_bytes_ += chunk_len;
      ESP_LOGV(TAG, "Received DFU Chunk #%" PRIu16 " (%zu bytes), Total Progress: %" PRIu32 "/%" PRIu32 " bytes",
               chunk_num, chunk_len, this->received_bytes_, this->blob_size_);
    } else {
      ESP_LOGE(TAG, "Error writing DFU Chunk #%" PRIu16 " to flash: code %d", chunk_num, res);
    }
  }
}

}  // namespace bluetooth_sig_mesh
}  // namespace esphome

#endif  // USE_BLUETOOTH_SIG_MESH
