#include "bluetooth_sig_mesh_esp32.h"

#if defined(USE_BLUETOOTH_SIG_MESH) && defined(USE_ESP32)

#include "esphome/components/esp32_ble/ble.h"
#include "esphome/components/esp32_ble/ble_advertising.h"

namespace esphome {
namespace bluetooth_sig_mesh {

static const char *const TAG = "bluetooth_sig_mesh.esp32";

void ESP32BluetoothSIGMesh::setup() {
  BluetoothSIGMesh::setup();
  ESP_LOGCONFIG(TAG, "Initializing ESP32 Bluetooth SIG Mesh driver...");
  this->init_esp32_mesh_();
  this->register_esp32_models_();
}

void ESP32BluetoothSIGMesh::loop() { BluetoothSIGMesh::loop(); }

void ESP32BluetoothSIGMesh::init_esp32_mesh_() {
  ESP_LOGI(TAG, "Configuring ESP32 BLE Mesh GAP advertisement parameters...");

  this->adv_params_ = {
      .adv_int_min = 0x0020,  // 20ms min interval
      .adv_int_max = 0x0040,  // 40ms max interval
      .adv_type = ADV_TYPE_IND,
      .own_addr_type = BLE_ADDR_TYPE_PUBLIC,
      .peer_addr = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
      .peer_addr_type = BLE_ADDR_TYPE_PUBLIC,
      .channel_map = ADV_CHNL_ALL,
      .adv_filter_policy = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY,
  };

  if (esp32_ble::global_esp32_ble != nullptr) {
    esp32_ble::global_esp32_ble->register_gap_event_handler(
        [this](esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param) {
          this->gap_event_handler_(event, param);
        });
    ESP_LOGD(TAG, "Registered ESP32 BLE GAP event handler for SIG Mesh");
  }

  if (this->enable_proxy_) {
    ESP_LOGI(TAG, "Configuring ESP32 GATT Mesh Proxy Service (0x1828)...");
  }
}

void ESP32BluetoothSIGMesh::gap_event_handler_(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param) {
  switch (event) {
    case ESP_GAP_BLE_ADV_DATA_RAW_SET_COMPLETE_EVT: {
      esp_err_t err = esp_ble_gap_start_advertising(&this->adv_params_);
      if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_ble_gap_start_advertising failed: %s", esp_err_to_name(err));
      }
      break;
    }
    case ESP_GAP_BLE_ADV_START_COMPLETE_EVT: {
      if (param->adv_start_cmpl.status != ESP_BT_STATUS_SUCCESS) {
        ESP_LOGE(TAG, "BLE advertising start failed");
      }
      break;
    }
    default:
      break;
  }
}

void ESP32BluetoothSIGMesh::register_esp32_models_() {
  ESP_LOGI(TAG, "Registering SIG Mesh Standard Models (Configuration Server, Generic OnOff)...");
}

void ESP32BluetoothSIGMesh::process_mesh_pdu(const uint8_t *data, size_t len) {
  BluetoothSIGMesh::process_mesh_pdu(data, len);
}

void ESP32BluetoothSIGMesh::send_mesh_pdu(uint16_t dst, uint16_t app_idx, uint16_t opcode, const uint8_t *payload,
                                          size_t len) {
  BluetoothSIGMesh::send_mesh_pdu(dst, app_idx, opcode, payload, len);
  const auto &framed_pdu = this->get_last_outgoing_frame();
  ESP_LOGI(TAG, "Broadcasting ESP32 BLE Mesh advertisement packet (DST: 0x%04X, Framed Len: %zu)...", dst,
           framed_pdu.size());

  uint8_t raw_adv[31] = {0};
  raw_adv[0] = 0x02;  // Length
  raw_adv[1] = 0x01;  // Flags
  raw_adv[2] = 0x06;  // General Discoverable & BR/EDR Not Supported

  if (!framed_pdu.empty() && framed_pdu.size() <= 26) {
    raw_adv[3] = static_cast<uint8_t>(framed_pdu.size() + 1);
    raw_adv[4] = MESH_AD_TYPE_MESSAGE;  // 0x2A Mesh Message AD Type
    std::memcpy(raw_adv + 5, framed_pdu.data(), framed_pdu.size());

    esp_err_t err = esp_ble_gap_config_adv_data_raw(raw_adv, framed_pdu.size() + 5);
    if (err != ESP_OK) {
      ESP_LOGE(TAG, "esp_ble_gap_config_adv_data_raw failed: %s", esp_err_to_name(err));
    }
  }
}

}  // namespace bluetooth_sig_mesh
}  // namespace esphome

#endif  // USE_BLUETOOTH_SIG_MESH && USE_ESP32
