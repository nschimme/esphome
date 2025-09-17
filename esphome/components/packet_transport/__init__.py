"""ESPHome packet transport component."""

import hashlib
import logging

import esphome.codegen as cg
from esphome.components.api import CONF_ENCRYPTION
from esphome.components.binary_sensor import BinarySensor
from esphome.components.sensor import Sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_BINARY_SENSORS,
    CONF_ID,
    CONF_INTERNAL,
    CONF_KEY,
    CONF_NAME,
    CONF_PLATFORM,
    CONF_SENSORS,
)
from esphome.core import CORE
from esphome.cpp_generator import MockObjClass
from esphome.components import http_request

CODEOWNERS = ["@clydebarrow"]
AUTO_LOAD = ["xxtea", "http_request"]

packet_transport_ns = cg.esphome_ns.namespace("packet_transport")
PacketTransport = packet_transport_ns.class_("PacketTransport", cg.PollingComponent)

IS_PLATFORM_COMPONENT = True

DOMAIN = "packet_transport"
CONF_BROADCAST = "broadcast"
CONF_BROADCAST_ID = "broadcast_id"
CONF_PROVIDER = "provider"
CONF_PROVIDERS = "providers"
CONF_REMOTE_ID = "remote_id"
CONF_PING_PONG_ENABLE = "ping_pong_enable"
CONF_PING_PONG_RECYCLE_TIME = "ping_pong_recycle_time"
CONF_ROLLING_CODE_ENABLE = "rolling_code_enable"
CONF_TRANSPORT_ID = "transport_id"
CONF_PROXY_LOGS_TO = "proxy_logs_to"


_LOGGER = logging.getLogger(__name__)


def sensor_validation(cls: MockObjClass):
    return cv.maybe_simple_value(
        cv.Schema(
            {
                cv.Required(CONF_ID): cv.use_id(cls),
                cv.Optional(CONF_BROADCAST_ID): cv.validate_id_name,
            }
        ),
        key=CONF_ID,
    )


def provider_name_validate(value):
    value = cv.valid_name(value)
    if "_" in value:
        _LOGGER.warning(
            "Device names typically do not contain underscores - did you mean to use a hyphen in '%s'?",
            value,
        )
    return value


ENCRYPTION_SCHEMA = {
    cv.Optional(CONF_ENCRYPTION): cv.maybe_simple_value(
        cv.Schema(
            {
                cv.Required(CONF_KEY): cv.string,
            }
        ),
        key=CONF_KEY,
    )
}

CONF_PROXY_LOGS = "proxy_logs"

PROVIDER_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_NAME): provider_name_validate,
        cv.Optional(CONF_PROXY_LOGS, default=False): cv.boolean,
    }
).extend(ENCRYPTION_SCHEMA)


def validate_(config):
    if CONF_ENCRYPTION in config:
        if CONF_SENSORS not in config and CONF_BINARY_SENSORS not in config:
            raise cv.Invalid("No sensors or binary sensors to encrypt")
    elif config[CONF_ROLLING_CODE_ENABLE]:
        raise cv.Invalid("Rolling code requires an encryption key")
    if config[CONF_PING_PONG_ENABLE] and not any(
        CONF_ENCRYPTION in p for p in config.get(CONF_PROVIDERS) or ()
    ):
        raise cv.Invalid("Ping-pong requires at least one encrypted provider")
    return config


TRANSPORT_SCHEMA = (
    cv.polling_component_schema("15s")
    .extend(
        {
            cv.Optional(CONF_ROLLING_CODE_ENABLE, default=False): cv.boolean,
            cv.Optional(CONF_PING_PONG_ENABLE, default=False): cv.boolean,
            cv.Optional(
                CONF_PING_PONG_RECYCLE_TIME, default="600s"
            ): cv.positive_time_period_seconds,
            cv.Optional(CONF_SENSORS): cv.ensure_list(sensor_validation(Sensor)),
            cv.Optional(CONF_BINARY_SENSORS): cv.ensure_list(
                sensor_validation(BinarySensor)
            ),
            cv.Optional(CONF_PROVIDERS, default=[]): cv.ensure_list(PROVIDER_SCHEMA),
            cv.Optional(CONF_PROXY_LOGS_TO): cv.string,
            cv.Optional("enable_remote_ota", default=False): cv.boolean,
        },
    )
    .extend(ENCRYPTION_SCHEMA)
    .add_extra(validate_)
)


def transport_schema(cls):
    return TRANSPORT_SCHEMA.extend({cv.GenerateID(): cv.declare_id(cls)})


# Build a list of sensors for this platform
CORE.data[DOMAIN] = {CONF_SENSORS: []}


def get_sensors(transport_id):
    """Return the list of sensors for this platform."""
    return (
        sensor
        for sensor in CORE.data[DOMAIN][CONF_SENSORS]
        if sensor[CONF_TRANSPORT_ID] == transport_id
    )


def validate_packet_transport_sensor(config):
    if CONF_NAME in config and CONF_INTERNAL not in config:
        raise cv.Invalid("Must provide internal: config when using name:")
    CORE.data[DOMAIN][CONF_SENSORS].append(config)
    return config


def packet_transport_sensor_schema(base_schema):
    return cv.All(
        base_schema.extend(
            {
                cv.GenerateID(CONF_TRANSPORT_ID): cv.use_id(PacketTransport),
                cv.Optional(CONF_REMOTE_ID): cv.string_strict,
                cv.Required(CONF_PROVIDER): provider_name_validate,
            }
        ),
        cv.has_at_least_one_key(CONF_ID, CONF_REMOTE_ID),
        validate_packet_transport_sensor,
    )


def hash_encryption_key(config: dict):
    return list(hashlib.sha256(config[CONF_KEY].encode()).digest())


async def register_packet_transport(var, config):
    var = await cg.register_component(var, config)
    cg.add(var.set_rolling_code_enable(config[CONF_ROLLING_CODE_ENABLE]))
    cg.add(var.set_ping_pong_enable(config[CONF_PING_PONG_ENABLE]))
    cg.add(
        var.set_ping_pong_recycle_time(
            config[CONF_PING_PONG_RECYCLE_TIME].total_seconds
        )
    )
    # Get directly configured providers, plus those from sensors and binary sensors
    providers = {
        sensor[CONF_PROVIDER] for sensor in get_sensors(config[CONF_ID])
    }.union(x[CONF_NAME] for x in config[CONF_PROVIDERS])
    for provider in providers:
        cg.add(var.add_provider(provider))
    for provider in config[CONF_PROVIDERS]:
        name = provider[CONF_NAME]
        if encryption := provider.get(CONF_ENCRYPTION):
            cg.add(var.set_provider_encryption(name, hash_encryption_key(encryption)))
        if provider.get(CONF_PROXY_LOGS):
            cg.add(var.set_on_log(cg.RawLambda(f"[](const std::string &target, int level, const char *tag, const char *message) {{ ESP_LOG_LW(level, tag, \"[%s] %s\", target.c_str(), message); }}")))

    for sens_conf in config.get(CONF_SENSORS, ()):
        sens_id = sens_conf[CONF_ID]
        sensor = await cg.get_variable(sens_id)
        bcst_id = sens_conf.get(CONF_BROADCAST_ID, sens_id.id)
        cg.add(var.add_sensor(bcst_id, sensor))
    for sens_conf in config.get(CONF_BINARY_SENSORS, ()):
        sens_id = sens_conf[CONF_ID]
        sensor = await cg.get_variable(sens_id)
        bcst_id = sens_conf.get(CONF_BROADCAST_ID, sens_id.id)
        cg.add(var.add_binary_sensor(bcst_id, sensor))

    if encryption := config.get(CONF_ENCRYPTION):
        cg.add(var.set_encryption_key(hash_encryption_key(encryption)))

    if target := config.get(CONF_PROXY_LOGS_TO):
        cg.add(cg.logger.get_global_logger().add_on_log_callback(cg.RawLambda(f"[](int level, const char *tag, const char *message) {{ {var}->send_log(\"{target}\", level, tag, message); }}")))

    if config.get("enable_remote_ota"):
        cg.add_library("Update", None)
        ota_backend = cg.new_Pvariable("ota_backend", cg.RawExpression("ota::make_ota_backend()"))

        cg.add(var.set_on_ota_begin(cg.RawLambda(f"[](const std::string &target, size_t size, const std::string &md5) {{ {ota_backend}->begin(size); {ota_backend}->set_update_md5(md5.c_str()); }}")))
        cg.add(var.set_on_ota_data(cg.RawLambda(f"[](const std::string &target, const std::vector<uint8_t> &data) {{ {ota_backend}->write(const_cast<uint8_t*>(data.data()), data.size()); }}")))
        cg.add(var.set_on_ota_end(cg.RawLambda(f"[](const std::string &target) {{ {ota_backend}->end(); }}")))

    return providers


async def new_packet_transport(config):
    var = cg.new_Pvariable(config[CONF_ID])
    cg.add(var.set_platform_name(config[CONF_PLATFORM]))
    providers = await register_packet_transport(var, config)
    return var, providers

CONF_PROXY_OTA_SERVICE = "proxy_ota"
CONF_URL = "url"
CONF_TARGET = "target"

AUTOMATION_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_ID): cv.use_id(PacketTransport),
        cv.Required(CONF_URL): cv.templatable(cv.url),
        cv.Required(CONF_TARGET): cv.templatable(cv.string),
    }
)

@automation.register_action(f"{DOMAIN}.{CONF_PROXY_OTA_SERVICE}", automation.Action, AUTOMATION_SCHEMA)
async def proxy_ota_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, paren)

    url = await cg.templatable(config[CONF_URL], args, cg.std_string)
    cg.add(var.set_url(url))

    target = await cg.templatable(config[CONF_TARGET], args, cg.std_string)
    cg.add(var.set_target(target))

    return var
