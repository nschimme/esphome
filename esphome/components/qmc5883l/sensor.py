import logging

from esphome import pins
import esphome.codegen as cg
from esphome.components import i2c, sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_ADDRESS,
    CONF_DATA_RATE,
    CONF_FIELD_STRENGTH_X,
    CONF_FIELD_STRENGTH_Y,
    CONF_FIELD_STRENGTH_Z,
    CONF_HEADING,
    CONF_ID,
    CONF_NOISE_LEVEL,
    CONF_OVERSAMPLING,
    CONF_RANGE,
    CONF_TEMPERATURE,
    CONF_UPDATE_INTERVAL,
    CONF_VARIANT,
    DEVICE_CLASS_TEMPERATURE,
    ICON_MAGNET,
    ICON_SCREEN_ROTATION,
    STATE_CLASS_MEASUREMENT,
    UNIT_CELSIUS,
    UNIT_DEGREES,
    UNIT_MICROTESLA,
)

_LOGGER = logging.getLogger(__name__)

CONF_DRDY_PIN = "drdy_pin"
CONF_SET_RESET_MODE = "set_reset_mode"
CONF_AXIS_SIGN = "axis_sign"

DEPENDENCIES = ["i2c"]

qmc5883l_ns = cg.esphome_ns.namespace("qmc5883l")

QMC5883LComponent = qmc5883l_ns.class_(
    "QMC5883LComponent", cg.PollingComponent, i2c.I2CDevice
)

QMC5883LVariant = qmc5883l_ns.enum("QMC5883LVariant")
VARIANTS = {
    "QMC5883L": QMC5883LVariant.QMC5883L_VARIANT_L,
    "QMC5883P": QMC5883LVariant.QMC5883L_VARIANT_P,
}

QMC5883LDatarate = qmc5883l_ns.enum("QMC5883LDatarate")
QMC5883LDatarates = {
    10: QMC5883LDatarate.QMC5883L_DATARATE_10_HZ,
    50: QMC5883LDatarate.QMC5883L_DATARATE_50_HZ,
    100: QMC5883LDatarate.QMC5883L_DATARATE_100_HZ,
    200: QMC5883LDatarate.QMC5883L_DATARATE_200_HZ,
}

QMC5883LRange = qmc5883l_ns.enum("QMC5883LRange")
QMC5883L_RANGES = {
    200: QMC5883LRange.QMC5883L_RANGE_200_UT,
    800: QMC5883LRange.QMC5883L_RANGE_800_UT,
    1200: QMC5883LRange.QMC5883L_RANGE_1200_UT,
    3000: QMC5883LRange.QMC5883L_RANGE_3000_UT,
}

QMC5883LOversampling = qmc5883l_ns.enum("QMC5883LOversampling")
QMC5883LOversamplings = {
    512: QMC5883LOversampling.QMC5883L_SAMPLING_512,
    256: QMC5883LOversampling.QMC5883L_SAMPLING_256,
    128: QMC5883LOversampling.QMC5883L_SAMPLING_128,
    64: QMC5883LOversampling.QMC5883L_SAMPLING_64,
    8: QMC5883LOversampling.QMC5883L_SAMPLING_8,
    4: QMC5883LOversampling.QMC5883L_SAMPLING_4,
    2: QMC5883LOversampling.QMC5883L_SAMPLING_2,
    1: QMC5883LOversampling.QMC5883L_SAMPLING_1,
}

QMC5883PNoiseLevel = qmc5883l_ns.enum("QMC5883PNoiseLevel")
QMC5883P_NOISE_LEVELS = {
    1: QMC5883PNoiseLevel.QMC5883P_NOISE_LEVEL_1,
    2: QMC5883PNoiseLevel.QMC5883P_NOISE_LEVEL_2,
    4: QMC5883PNoiseLevel.QMC5883P_NOISE_LEVEL_4,
    8: QMC5883PNoiseLevel.QMC5883P_NOISE_LEVEL_8,
}

QMC5883PSetResetMode = qmc5883l_ns.enum("QMC5883PSetResetMode")
QMC5883P_SET_RESET_MODES = {
    "BOTH": QMC5883PSetResetMode.QMC5883P_SET_RESET_BOTH,
    "SET_ONLY": QMC5883PSetResetMode.QMC5883P_SET_RESET_SET_ONLY,
    "OFF": QMC5883PSetResetMode.QMC5883P_SET_RESET_OFF,
}


def validate_config(config):
    variant = config[CONF_VARIANT]
    if variant == "QMC5883L":
        if CONF_RANGE in config and config[CONF_RANGE] > 800:
            raise cv.Invalid("QMC5883L only supports ranges up to 800µT")
        if CONF_OVERSAMPLING in config and config[CONF_OVERSAMPLING] < 64:
            raise cv.Invalid("QMC5883L oversampling must be 64x, 128x, 256x, or 512x")
        if CONF_NOISE_LEVEL in config:
            raise cv.Invalid("QMC5883L does not support noise_level")
        if CONF_SET_RESET_MODE in config:
            raise cv.Invalid("QMC5883L does not support set_reset_mode")
    elif variant == "QMC5883P":
        if CONF_OVERSAMPLING in config and config[CONF_OVERSAMPLING] > 8:
            raise cv.Invalid("QMC5883P oversampling must be 1x, 2x, 4x, or 8x")
        if CONF_TEMPERATURE in config:
            _LOGGER.warning("QMC5883P does not support temperature sensor")

    if (
        config[CONF_UPDATE_INTERVAL].total_milliseconds < 15
        and CONF_DRDY_PIN not in config
    ):
        _LOGGER.warning(
            "[qmc5883l] 'update_interval' is less than 15ms and 'drdy_pin' is "
            "not configured, this may result in I2C errors"
        )
    return config


def validate_enum(enum_values, units=None, int=True):
    _units = []
    if units is not None:
        _units = units if isinstance(units, list) else [units]
        _units = [str(x) for x in _units]
    enum_bound = cv.enum(enum_values, int=int)

    def validate_enum_bound(value):
        value = cv.string(value)
        for unit in _units:
            if value.endswith(unit):
                value = value[: -len(unit)]
                break
        return enum_bound(value)

    return validate_enum_bound


field_strength_schema = sensor.sensor_schema(
    unit_of_measurement=UNIT_MICROTESLA,
    icon=ICON_MAGNET,
    accuracy_decimals=1,
    state_class=STATE_CLASS_MEASUREMENT,
)
heading_schema = sensor.sensor_schema(
    unit_of_measurement=UNIT_DEGREES,
    icon=ICON_SCREEN_ROTATION,
    accuracy_decimals=1,
    state_class=STATE_CLASS_MEASUREMENT,
)
temperature_schema = sensor.sensor_schema(
    unit_of_measurement=UNIT_CELSIUS,
    accuracy_decimals=1,
    device_class=DEVICE_CLASS_TEMPERATURE,
    state_class=STATE_CLASS_MEASUREMENT,
)

CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(QMC5883LComponent),
            cv.Optional(CONF_VARIANT, default="QMC5883L"): cv.enum(VARIANTS, upper=True),
            cv.Optional(CONF_ADDRESS): cv.i2c_address,
            cv.Optional(CONF_RANGE): validate_enum(QMC5883L_RANGES, units=["uT", "µT"]),
            cv.Optional(CONF_OVERSAMPLING): validate_enum(
                QMC5883LOversamplings, units="x"
            ),
            cv.Optional(CONF_NOISE_LEVEL): validate_enum(
                QMC5883P_NOISE_LEVELS, units="x"
            ),
            cv.Optional(CONF_SET_RESET_MODE): cv.enum(
                QMC5883P_SET_RESET_MODES, upper=True
            ),
            cv.Optional(CONF_AXIS_SIGN): cv.uint8_t,
            cv.Optional(CONF_FIELD_STRENGTH_X): field_strength_schema,
            cv.Optional(CONF_FIELD_STRENGTH_Y): field_strength_schema,
            cv.Optional(CONF_FIELD_STRENGTH_Z): field_strength_schema,
            cv.Optional(CONF_HEADING): heading_schema,
            cv.Optional(CONF_TEMPERATURE): temperature_schema,
            cv.Optional(CONF_DRDY_PIN): pins.gpio_input_pin_schema,
            cv.Optional(CONF_DATA_RATE, default="200hz"): validate_enum(
                QMC5883LDatarates, units=["hz", "Hz"]
            ),
        }
    )
    .extend(cv.polling_component_schema("60s"))
    .extend(i2c.i2c_device_schema(0x0D)),
    cv.has_none_or_all_keys(CONF_VARIANT),
    validate_config,
)


def final_validate_config(config):
    variant = config[CONF_VARIANT]
    if CONF_RANGE not in config:
        config[CONF_RANGE] = 200
    if CONF_OVERSAMPLING not in config:
        config[CONF_OVERSAMPLING] = 512 if variant == "QMC5883L" else 8
    if variant == "QMC5883P":
        if CONF_NOISE_LEVEL not in config:
            config[CONF_NOISE_LEVEL] = 8
        if CONF_SET_RESET_MODE not in config:
            config[CONF_SET_RESET_MODE] = "BOTH"
        if CONF_AXIS_SIGN not in config:
            config[CONF_AXIS_SIGN] = 0x06

    if variant == "QMC5883P" and config.get(CONF_ADDRESS) == 0x0D:
        config[CONF_ADDRESS] = 0x2C

    return config


FINAL_VALIDATE_SCHEMA = final_validate_config


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)

    cg.add(var.set_variant(config[CONF_VARIANT]))
    cg.add(var.set_oversampling(config[CONF_OVERSAMPLING]))
    cg.add(var.set_datarate(config[CONF_DATA_RATE]))
    cg.add(var.set_range(config[CONF_RANGE]))

    if config[CONF_VARIANT] == "QMC5883P":
        cg.add(var.set_noise_level(config[CONF_NOISE_LEVEL]))
        cg.add(
            var.set_set_reset_mode(QMC5883P_SET_RESET_MODES[config[CONF_SET_RESET_MODE]])
        )
        cg.add(var.set_axis_sign(config[CONF_AXIS_SIGN]))

    if CONF_FIELD_STRENGTH_X in config:
        sens = await sensor.new_sensor(config[CONF_FIELD_STRENGTH_X])
        cg.add(var.set_x_sensor(sens))
    if CONF_FIELD_STRENGTH_Y in config:
        sens = await sensor.new_sensor(config[CONF_FIELD_STRENGTH_Y])
        cg.add(var.set_y_sensor(sens))
    if CONF_FIELD_STRENGTH_Z in config:
        sens = await sensor.new_sensor(config[CONF_FIELD_STRENGTH_Z])
        cg.add(var.set_z_sensor(sens))
    if CONF_HEADING in config:
        sens = await sensor.new_sensor(config[CONF_HEADING])
        cg.add(var.set_heading_sensor(sens))
    if CONF_TEMPERATURE in config:
        sens = await sensor.new_sensor(config[CONF_TEMPERATURE])
        cg.add(var.set_temperature_sensor(sens))
    if CONF_DRDY_PIN in config:
        pin = await cg.gpio_pin_expression(config[CONF_DRDY_PIN])
        cg.add(var.set_drdy_pin(pin))
