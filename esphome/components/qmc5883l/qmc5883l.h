#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/i2c/i2c.h"
#include "esphome/core/hal.h"

namespace esphome {
namespace qmc5883l {

enum QMC5883LVariant {
  QMC5883L_VARIANT_L = 0,
  QMC5883L_VARIANT_P = 1,
};

enum QMC5883LDatarate {
  QMC5883L_DATARATE_10_HZ = 0b00,
  QMC5883L_DATARATE_50_HZ = 0b01,
  QMC5883L_DATARATE_100_HZ = 0b10,
  QMC5883L_DATARATE_200_HZ = 0b11,
};

enum QMC5883LRange {
  QMC5883L_RANGE_200_UT = 0b00,
  QMC5883L_RANGE_800_UT = 0b01,
  QMC5883L_RANGE_1200_UT = 0b10,
  QMC5883L_RANGE_3000_UT = 0b11,
};

enum QMC5883LOversampling {
  QMC5883L_SAMPLING_512 = 0b00,
  QMC5883L_SAMPLING_256 = 0b01,
  QMC5883L_SAMPLING_128 = 0b10,
  QMC5883L_SAMPLING_64 = 0b11,
  QMC5883L_SAMPLING_8 = 0b00,
  QMC5883L_SAMPLING_4 = 0b01,
  QMC5883L_SAMPLING_2 = 0b10,
  QMC5883L_SAMPLING_1 = 0b11,
};

enum QMC5883PNoiseLevel {
  QMC5883P_NOISE_LEVEL_1 = 0b00,
  QMC5883P_NOISE_LEVEL_2 = 0b01,
  QMC5883P_NOISE_LEVEL_4 = 0b10,
  QMC5883P_NOISE_LEVEL_8 = 0b11,
};

enum QMC5883PSetResetMode {
  QMC5883P_SET_RESET_BOTH = 0b00,
  QMC5883P_SET_RESET_SET_ONLY = 0b01,
  QMC5883P_SET_RESET_OFF = 0b10,
};

class QMC5883LComponent : public PollingComponent, public i2c::I2CDevice {
 public:
  void setup() override;
  void dump_config() override;
  void update() override;
  void loop() override;

  void set_variant(QMC5883LVariant variant) { this->variant_ = variant; }
  void set_drdy_pin(GPIOPin *pin) { this->drdy_pin_ = pin; }
  void set_datarate(QMC5883LDatarate datarate) { this->datarate_ = datarate; }
  void set_range(QMC5883LRange range) { this->range_ = range; }
  void set_oversampling(QMC5883LOversampling oversampling) { this->oversampling_ = oversampling; }
  void set_noise_level(QMC5883PNoiseLevel noise_level) { this->noise_level_ = noise_level; }
  void set_set_reset_mode(QMC5883PSetResetMode mode) { this->set_reset_mode_ = mode; }
  void set_axis_sign(uint8_t axis_sign) { this->axis_sign_ = axis_sign; }
  void set_x_sensor(sensor::Sensor *x_sensor) { this->x_sensor_ = x_sensor; }
  void set_y_sensor(sensor::Sensor *y_sensor) { this->y_sensor_ = y_sensor; }
  void set_z_sensor(sensor::Sensor *z_sensor) { this->z_sensor_ = z_sensor; }
  void set_heading_sensor(sensor::Sensor *heading_sensor) { this->heading_sensor_ = heading_sensor; }
  void set_temperature_sensor(sensor::Sensor *temperature_sensor) { this->temperature_sensor_ = temperature_sensor; }

 protected:
  static void IRAM_ATTR gpio_intr(QMC5883LComponent *arg);
  void read_sensor_();

  QMC5883LVariant variant_{QMC5883L_VARIANT_L};
  QMC5883LDatarate datarate_{QMC5883L_DATARATE_10_HZ};
  QMC5883LRange range_{QMC5883L_RANGE_200_UT};
  QMC5883LOversampling oversampling_{QMC5883L_SAMPLING_512};
  QMC5883PNoiseLevel noise_level_{QMC5883P_NOISE_LEVEL_8};
  QMC5883PSetResetMode set_reset_mode_{QMC5883P_SET_RESET_BOTH};
  uint8_t axis_sign_{0x06};
  sensor::Sensor *x_sensor_{nullptr};
  sensor::Sensor *y_sensor_{nullptr};
  sensor::Sensor *z_sensor_{nullptr};
  sensor::Sensor *heading_sensor_{nullptr};
  sensor::Sensor *temperature_sensor_{nullptr};
  GPIOPin *drdy_pin_{nullptr};
  bool drdy_use_isr_{false};
  enum ErrorCode {
    NONE = 0,
    COMMUNICATION_FAILED,
    WRONG_ID,
  } error_code_{NONE};
  i2c::ErrorCode read_bytes_16_le_(uint8_t a_register, uint16_t *data, uint8_t len = 1);
  HighFrequencyLoopRequester high_freq_;
};

}  // namespace qmc5883l
}  // namespace esphome
