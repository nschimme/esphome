#include "ota_packet_transport.h"
#include "esphome/core/log.h"
#include "esphome/core/application.h"

#include <vector>

namespace esphome {
namespace packet_transport {

static const char *const TAG = "packet_transport.ota";

void PacketTransportOTAComponent::handle_ota_packet(const std::vector<uint8_t> &data) {
  if (data.empty()) {
    ESP_LOGW(TAG, "Received empty OTA packet");
    return;
  }

  uint8_t packet_type = data[0];
  std::vector<uint8_t> payload(data.begin() + 1, data.end());

  switch (packet_type) {
    case 0x00: // START
      this->on_ota_start_(payload);
      break;
    case 0x01: // DATA
      this->on_ota_data_(payload);
      break;
    case 0x02: // END
      this->on_ota_end_();
      break;
    case 0x03: // ABORT
      this->on_ota_abort_();
      break;
    default:
      ESP_LOGW(TAG, "Unknown OTA packet type: 0x%02X", packet_type);
      break;
  }
}

void PacketTransportOTAComponent::on_ota_start_(const std::vector<uint8_t> &data) {
  if (this->ota_backend_) {
    ESP_LOGW(TAG, "OTA already in progress. Aborting.");
    this->on_ota_abort_();
  }

  if (data.size() != 36) {
    ESP_LOGE(TAG, "Invalid OTA start packet size: %zu", data.size());
    return;
  }

  this->ota_size_ = (data[0] | (data[1] << 8) | (data[2] << 16) | (data[3] << 24));
  std::string md5_str(reinterpret_cast<const char*>(data.data() + 4), 32);

  ESP_LOGI(TAG, "Starting OTA update. Size: %u, MD5: %s", this->ota_size_, md5_str.c_str());

  this->ota_backend_ = ota::make_ota_backend();
  if (!this->ota_backend_) {
    ESP_LOGE(TAG, "Could not create OTA backend!");
    return;
  }

  this->ota_backend_->set_update_md5(md5_str.c_str());
  auto err = this->ota_backend_->begin(this->ota_size_);
  if (err != ota::OTA_RESPONSE_OK) {
    ESP_LOGE(TAG, "Error starting OTA: %d", err);
    this->ota_backend_.reset();
    return;
  }

  this->ota_written_ = 0;
  this->state_callback_.call(ota::OTA_STARTED, 0.0f, 0);
}

void PacketTransportOTAComponent::on_ota_data_(const std::vector<uint8_t> &data) {
  if (!this->ota_backend_) {
    ESP_LOGW(TAG, "OTA not in progress. Ignoring data packet.");
    return;
  }

  auto err = this->ota_backend_->write(const_cast<uint8_t*>(data.data()), data.size());
  if (err != ota::OTA_RESPONSE_OK) {
    ESP_LOGE(TAG, "Error writing OTA data: %d", err);
    this->on_ota_abort_();
    return;
  }

  this->ota_written_ += data.size();
  float progress = (this->ota_written_ * 100.0f) / this->ota_size_;
  this->state_callback_.call(ota::OTA_IN_PROGRESS, progress, 0);
}

void PacketTransportOTAComponent::on_ota_end_() {
  if (!this->ota_backend_) {
    ESP_LOGW(TAG, "OTA not in progress. Ignoring end packet.");
    return;
  }

  if (this->ota_written_ != this->ota_size_) {
    ESP_LOGE(TAG, "OTA written size mismatch. Expected %u, got %u", this->ota_size_, this->ota_written_);
    this->on_ota_abort_();
    return;
  }

  auto err = this->ota_backend_->end();
  if (err != ota::OTA_RESPONSE_OK) {
    ESP_LOGE(TAG, "Error ending OTA: %d", err);
    this->on_ota_abort_();
    return;
  }

  this->state_callback_.call(ota::OTA_COMPLETED, 100.0f, 0);
  this->ota_backend_.reset();

  ESP_LOGI(TAG, "OTA update finished! Rebooting...");
  Application.safe_reboot();
}

void PacketTransportOTAComponent::on_ota_abort_() {
  if (this->ota_backend_) {
    this->ota_backend_->abort();
    this->ota_backend_.reset();
    this->state_callback_.call(ota::OTA_ABORT, 0.0f, 0);
    ESP_LOGW(TAG, "OTA update aborted.");
  }
}

}  // namespace packet_transport
}  // namespace esphome
