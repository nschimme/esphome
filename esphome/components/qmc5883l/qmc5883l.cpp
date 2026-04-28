#include "qmc5883l.h"
#include "esphome/core/application.h"
#include "esphome/core/log.h"
#include "esphome/core/hal.h"
#include <cmath>

namespace esphome {
namespace qmc5883l {

static const char *const TAG = "qmc5883l";

static const uint8_t QMC5883L_REGISTER_DATA_X_LSB = 0x00;
static const uint8_t QMC5883L_REGISTER_STATUS = 0x06;
static const uint8_t QMC5883L_REGISTER_TEMPERATURE_LSB = 0x07;
static const uint8_t QMC5883L_REGISTER_CONTROL_1 = 0x09;
static const uint8_t QMC5883L_REGISTER_CONTROL_2 = 0x0A;
static const uint8_t QMC5883L_REGISTER_PERIOD = 0x0B;
static const uint8_t QMC5883L_REGISTER_CHIP_ID = 0x0D;

static const uint8_t QMC5883P_REGISTER_CHIP_ID = 0x00;
static const uint8_t QMC5883P_REGISTER_DATA_X_LSB = 0x01;
static const uint8_t QMC5883P_REGISTER_STATUS = 0x09;
static const uint8_t QMC5883P_REGISTER_CONTROL_1 = 0x0A;
static const uint8_t QMC5883P_REGISTER_CONTROL_2 = 0x0B;
static const uint8_t QMC5883P_REGISTER_PERIOD = 0x0C;
static const uint8_t QMC5883P_REGISTER_AXIS_SIGN = 0x29;

void IRAM_ATTR QMC5883LComponent::gpio_intr(QMC5883LComponent *arg) { arg->enable_loop_soon_any_context(); }

void QMC5883LComponent::setup() {
  uint8_t chip_id;
  bool ok;
  if (this->variant_ == QMC5883L_VARIANT_L) {
    ok = this->read_byte(QMC5883L_REGISTER_CHIP_ID, &chip_id);
    if (ok && chip_id != 0xFF) {
      ESP_LOGW(TAG, "Chip ID 0x%02X does not match QMC5883L (0xFF)", chip_id);
      this->error_code_ = WRONG_ID;
      this->mark_failed();
      return;
    }
  } else {
    ok = this->read_byte(QMC5883P_REGISTER_CHIP_ID, &chip_id);
    if (ok && chip_id != 0x80) {
      ESP_LOGW(TAG, "Chip ID 0x%02X does not match QMC5883P (0x80)", chip_id);
      this->error_code_ = WRONG_ID;
      this->mark_failed();
      return;
    }
  }

  if (!ok) {
    this->error_code_ = COMMUNICATION_FAILED;
    this->mark_failed();
    return;
  }

  // Soft Reset
  if (this->variant_ == QMC5883L_VARIANT_L) {
    if (!this->write_byte(QMC5883L_REGISTER_CONTROL_2, 1 << 7)) {
      this->error_code_ = COMMUNICATION_FAILED;
      this->mark_failed();
      return;
    }
  } else {
    if (!this->write_byte(QMC5883P_REGISTER_CONTROL_2, 1 << 7)) {
      this->error_code_ = COMMUNICATION_FAILED;
      this->mark_failed();
      return;
    }
  }
  delay(10);

  if (this->drdy_pin_) {
    this->drdy_pin_->setup();
    if (this->drdy_pin_->is_internal()) {
      static_cast<InternalGPIOPin *>(this->drdy_pin_)
          ->attach_interrupt(&QMC5883LComponent::gpio_intr, this, gpio::INTERRUPT_RISING_EDGE);
      this->drdy_use_isr_ = true;
      this->stop_poller();
    }
  }

  if (this->variant_ == QMC5883L_VARIANT_L) {
    uint8_t control_1 = 0;
    control_1 |= 0b01 << 0;  // MODE (Mode) -> 0b00=standby, 0b01=continuous
    control_1 |= this->datarate_ << 2;
    control_1 |= this->range_ << 4;
    control_1 |= this->oversampling_ << 6;
    if (!this->write_byte(QMC5883L_REGISTER_CONTROL_1, control_1)) {
      this->error_code_ = COMMUNICATION_FAILED;
      this->mark_failed();
      return;
    }

    uint8_t control_2 = 0;
    control_2 |= 0b0 << 7;  // SOFT_RST (Soft Reset) -> 0b00=disabled, 0b01=enabled
    control_2 |= 0b0 << 6;  // ROL_PNT (Pointer Roll Over) -> 0b00=disabled, 0b01=enabled
    control_2 |= 0b0 << 0;  // INT_ENB (Interrupt) -> 0b00=disabled, 0b01=enabled
    if (!this->write_byte(QMC5883L_REGISTER_CONTROL_2, control_2)) {
      this->error_code_ = COMMUNICATION_FAILED;
      this->mark_failed();
      return;
    }

    uint8_t period = 0x01;  // recommended value
    if (!this->write_byte(QMC5883L_REGISTER_PERIOD, period)) {
      this->error_code_ = COMMUNICATION_FAILED;
      this->mark_failed();
      return;
    }
  } else {
    // QMC5883P Initialization
    if (!this->write_byte(QMC5883P_REGISTER_AXIS_SIGN, this->axis_sign_)) {
      this->error_code_ = COMMUNICATION_FAILED;
      this->mark_failed();
      return;
    }

    uint8_t control_1 = 0;
    control_1 |= 0b11 << 0;  // MODE -> 0b11=continuous
    control_1 |= this->datarate_ << 2;
    control_1 |= this->oversampling_ << 4;  // OSR1
    control_1 |= this->noise_level_ << 6;   // OSR2
    if (!this->write_byte(QMC5883P_REGISTER_CONTROL_1, control_1)) {
      this->error_code_ = COMMUNICATION_FAILED;
      this->mark_failed();
      return;
    }

    uint8_t control_2 = 0;
    control_2 |= 0b0 << 7;  // SOFT_RST
    control_2 |= 0b0 << 6;  // SELF_TEST
    // RNG: 30G=00, 12G=01, 8G=10, 2G=11. Enum is 2G=0, 8G=1, 12G=2, 30G=3
    control_2 |= (3 - this->range_) << 2;
    control_2 |= this->set_reset_mode_;  // SET/RESET MODE
    if (!this->write_byte(QMC5883P_REGISTER_CONTROL_2, control_2)) {
      this->error_code_ = COMMUNICATION_FAILED;
      this->mark_failed();
      return;
    }
  }

  if (!this->drdy_use_isr_ && this->get_update_interval() < App.get_loop_interval()) {
    this->high_freq_.start();
  }
}

void QMC5883LComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "QMC5883L:");
  LOG_I2C_DEVICE(this);
  if (this->error_code_ == COMMUNICATION_FAILED) {
    ESP_LOGE(TAG, ESP_LOG_MSG_COMM_FAIL);
  } else if (this->error_code_ == WRONG_ID) {
    ESP_LOGE(TAG, "Chip ID did not match variant!");
  }
  LOG_UPDATE_INTERVAL(this);

  ESP_LOGCONFIG(TAG, "  Variant: %s", (this->variant_ == QMC5883L_VARIANT_L) ? "QMC5883L" : "QMC5883P");

  LOG_SENSOR("  ", "X Axis", this->x_sensor_);
  LOG_SENSOR("  ", "Y Axis", this->y_sensor_);
  LOG_SENSOR("  ", "Z Axis", this->z_sensor_);
  LOG_SENSOR("  ", "Heading", this->heading_sensor_);
  if (this->variant_ == QMC5883L_VARIANT_L) {
    LOG_SENSOR("  ", "Temperature", this->temperature_sensor_);
  }
  LOG_PIN("  DRDY Pin: ", this->drdy_pin_);
  if (this->drdy_pin_ != nullptr) {
    ESP_LOGCONFIG(TAG, "  DRDY mode: %s",
                  this->drdy_use_isr_ ? LOG_STR_LITERAL("interrupt") : LOG_STR_LITERAL("polling"));
  }
}

void QMC5883LComponent::update() {
  // If DRDY is on an external expander we keep the polling path and early-return
  // if data is not ready yet. Internal DRDY pins take the ISR path via loop().
  if (this->drdy_pin_ && !this->drdy_pin_->digital_read()) {
    return;
  }
  this->read_sensor_();
}

void QMC5883LComponent::loop() {
  this->disable_loop();
  if (!this->drdy_use_isr_ || !this->drdy_pin_->digital_read()) {
    return;
  }
  this->read_sensor_();
}

void QMC5883LComponent::read_sensor_() {
  i2c::ErrorCode err;
  uint8_t status = false;

  uint8_t status_reg = (this->variant_ == QMC5883L_VARIANT_L) ? QMC5883L_REGISTER_STATUS : QMC5883P_REGISTER_STATUS;

  if (ESPHOME_LOG_LEVEL >= ESPHOME_LOG_LEVEL_VERBOSE) {
    err = this->read_register(status_reg, &status, 1);
    if (err != i2c::ERROR_OK) {
      ESP_LOGW(TAG, "status read failed (%d)", err);
      this->status_set_warning();
      return;
    }
  }

  uint16_t raw[3] = {0};
  uint8_t start, dest;
  uint8_t data_x_lsb =
      (this->variant_ == QMC5883L_VARIANT_L) ? QMC5883L_REGISTER_DATA_X_LSB : QMC5883P_REGISTER_DATA_X_LSB;

  if (this->heading_sensor_ != nullptr || this->x_sensor_ != nullptr) {
    start = data_x_lsb;
    dest = 0;
  } else if (this->y_sensor_ != nullptr) {
    start = data_x_lsb + 2;
    dest = 1;
  } else {
    start = data_x_lsb + 4;
    dest = 2;
  }
  err = this->read_bytes_16_le_(start, &raw[dest], 3 - dest);
  if (err != i2c::ERROR_OK) {
    ESP_LOGW(TAG, "mag read failed (%d)", err);
    this->status_set_warning();
    return;
  }

  float mg_per_bit;
  if (this->variant_ == QMC5883L_VARIANT_L) {
    switch (this->range_) {
      case QMC5883L_RANGE_200_UT:
        mg_per_bit = 0.0833f;
        break;
      case QMC5883L_RANGE_800_UT:
        mg_per_bit = 0.333f;
        break;
      default:
        mg_per_bit = NAN;
    }
  } else {
    switch (this->range_) {
      case QMC5883L_RANGE_200_UT:
        mg_per_bit = 2000.0f / 32768.0f;
        break;
      case QMC5883L_RANGE_800_UT:
        mg_per_bit = 8000.0f / 32768.0f;
        break;
      case QMC5883L_RANGE_1200_UT:
        mg_per_bit = 12000.0f / 32768.0f;
        break;
      case QMC5883L_RANGE_3000_UT:
        mg_per_bit = 30000.0f / 32768.0f;
        break;
      default:
        mg_per_bit = NAN;
    }
  }

  // in µT
  const float x = int16_t(raw[0]) * mg_per_bit * 0.1f;
  const float y = int16_t(raw[1]) * mg_per_bit * 0.1f;
  const float z = int16_t(raw[2]) * mg_per_bit * 0.1f;

  float heading = atan2f(0.0f - x, y) * 180.0f / M_PI;

  float temp = NAN;
  if (this->variant_ == QMC5883L_VARIANT_L && this->temperature_sensor_ != nullptr) {
    uint16_t raw_temp;
    err = this->read_bytes_16_le_(QMC5883L_REGISTER_TEMPERATURE_LSB, &raw_temp);
    if (err != i2c::ERROR_OK) {
      ESP_LOGW(TAG, "temp read failed (%d)", err);
      this->status_set_warning();
      return;
    }
    temp = int16_t(raw_temp) * 0.01f;
  }

  ESP_LOGV(TAG, "Got x=%0.02fµT y=%0.02fµT z=%0.02fµT heading=%0.01f° temperature=%0.01f°C status=%u", x, y, z, heading,
           temp, status);

  if (this->x_sensor_ != nullptr)
    this->x_sensor_->publish_state(x);
  if (this->y_sensor_ != nullptr)
    this->y_sensor_->publish_state(y);
  if (this->z_sensor_ != nullptr)
    this->z_sensor_->publish_state(z);
  if (this->heading_sensor_ != nullptr)
    this->heading_sensor_->publish_state(heading);
  if (this->temperature_sensor_ != nullptr && !std::isnan(temp))
    this->temperature_sensor_->publish_state(temp);
}

i2c::ErrorCode QMC5883LComponent::read_bytes_16_le_(uint8_t a_register, uint16_t *data, uint8_t len) {
  i2c::ErrorCode err = this->read_register(a_register, reinterpret_cast<uint8_t *>(data), len * 2);
  if (err != i2c::ERROR_OK)
    return err;
  for (size_t i = 0; i < len; i++)
    data[i] = convert_little_endian(data[i]);
  return err;
}

}  // namespace qmc5883l
}  // namespace esphome
