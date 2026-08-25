#pragma once

#ifdef USE_BLUETOOTH_SIG_MESH

#if defined(USE_OTA)
#include "esphome/components/ota/ota_backend.h"
#include "esphome/components/ota/ota_backend_factory.h"
#endif
#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

#include <cstdint>
#include <memory>
#include <vector>

namespace esphome {
namespace bluetooth_sig_mesh {

// SIG Mesh DFU & BLOB Transfer Opcodes
constexpr uint16_t OPCODE_FW_UPDATE_INFO_GET = 0xB601;
constexpr uint16_t OPCODE_FW_UPDATE_INFO_STATUS = 0xB602;
constexpr uint16_t OPCODE_FW_UPDATE_START = 0xB603;
constexpr uint16_t OPCODE_FW_UPDATE_STATUS = 0xB604;
constexpr uint16_t OPCODE_FW_UPDATE_APPLY = 0xB605;

constexpr uint16_t OPCODE_BLOB_TRANSFER_GET = 0xB701;
constexpr uint16_t OPCODE_BLOB_TRANSFER_START = 0xB702;
constexpr uint16_t OPCODE_BLOB_TRANSFER_STATUS = 0xB703;
constexpr uint16_t OPCODE_BLOB_BLOCK_START = 0xB704;
constexpr uint16_t OPCODE_BLOB_BLOCK_STATUS = 0xB705;
constexpr uint16_t OPCODE_BLOB_CHUNK_TRANSFER = 0x007D;
constexpr uint16_t OPCODE_BLOB_INFO_GET = 0xB70A;
constexpr uint16_t OPCODE_BLOB_INFO_STATUS = 0xB70B;

enum class DFUState : uint8_t {
  IDLE = 0,
  TRANSFER_IN_PROGRESS = 1,
  BLOCK_IN_PROGRESS = 2,
  TRANSFER_COMPLETE = 3,
  APPLY_PENDING = 4,
  FAILED = 5,
};

class BluetoothSIGMesh;

class BluetoothSIGMeshDFUServer {
 public:
  BluetoothSIGMeshDFUServer() = default;

  void set_mesh(BluetoothSIGMesh *mesh) { this->mesh_ = mesh; }

  bool handle_dfu_opcode(uint16_t src, uint16_t opcode, const uint8_t *payload, size_t len);

 protected:
  void handle_fw_info_get_(uint16_t src);
  void handle_fw_update_start_(uint16_t src, const uint8_t *payload, size_t len);
  void handle_fw_update_apply_(uint16_t src);

  void handle_blob_info_get_(uint16_t src);
  void handle_blob_transfer_start_(uint16_t src, const uint8_t *payload, size_t len);
  void handle_blob_block_start_(uint16_t src, const uint8_t *payload, size_t len);
  void handle_blob_chunk_transfer_(uint16_t src, const uint8_t *payload, size_t len);

  BluetoothSIGMesh *mesh_{nullptr};
  DFUState dfu_state_{DFUState::IDLE};

  uint64_t blob_id_{0};
  uint32_t blob_size_{0};
  uint16_t block_size_{4096};
  uint16_t chunk_size_{32};
  uint16_t current_block_num_{0};
  uint32_t received_bytes_{0};

#if defined(USE_OTA)
  ota::OTABackendPtr ota_backend_{nullptr};
#endif
};

}  // namespace bluetooth_sig_mesh
}  // namespace esphome

#endif  // USE_BLUETOOTH_SIG_MESH
