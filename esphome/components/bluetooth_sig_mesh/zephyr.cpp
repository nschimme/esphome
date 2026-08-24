#include "zephyr.h"

#if defined(USE_BLUETOOTH_SIG_MESH) && defined(USE_ZEPHYR)

#include "esphome/core/log.h"
#include <cstring>

namespace esphome {
namespace bluetooth_sig_mesh {

static const char *const TAG = "bluetooth_sig_mesh.zephyr";

void ZephyrBluetoothSIGMesh::setup() {
  BluetoothSIGMesh::setup();
  ESP_LOGCONFIG(TAG, "Initializing Zephyr RTOS Bluetooth SIG Mesh driver...");
  this->init_zephyr_mesh_();
}

void ZephyrBluetoothSIGMesh::loop() { BluetoothSIGMesh::loop(); }

void ZephyrBluetoothSIGMesh::init_zephyr_mesh_() {
  ESP_LOGI(TAG, "Configuring Zephyr BLE Subsystem GAP parameters...");
}

void ZephyrBluetoothSIGMesh::transmit_last_outgoing_frame() {
  const uint8_t *pdu_data = this->get_last_outgoing_frame_data();
  size_t pdu_len = this->get_last_outgoing_frame_len();
  if (pdu_len == 0 || pdu_data == nullptr || pdu_len > 26) {
    return;
  }

  uint8_t flags = BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR;
  struct bt_data ad[] = {
      BT_DATA(BT_DATA_FLAGS, &flags, sizeof(flags)),
      BT_DATA(MESH_AD_TYPE_MESSAGE, pdu_data, pdu_len),
  };

  struct bt_le_adv_param adv_param = *BT_LE_ADV_NCONN;
  adv_param.interval_min = 0x0020;
  adv_param.interval_max = 0x0040;

  int err = bt_le_adv_start(&adv_param, ad, ARRAY_SIZE(ad), nullptr, 0);
  if (err && err != -EALREADY) {
    ESP_LOGE(TAG, "bt_le_adv_start failed: %d", err);
  }
}

}  // namespace bluetooth_sig_mesh
}  // namespace esphome

#endif  // USE_BLUETOOTH_SIG_MESH && USE_ZEPHYR
