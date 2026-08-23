import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import (
    CONF_ID,
    PLATFORM_BK72XX,
    PLATFORM_ESP32,
    PLATFORM_LN882X,
    PLATFORM_RP2,
)
from esphome.core import CORE
from esphome.types import ConfigType

CODEOWNERS = ["@esphome"]

CONF_MESH_ROLE = "mesh_role"
CONF_ENABLE_NODE = "enable_node"
CONF_ENABLE_PROXY = "enable_proxy"
CONF_PROVISIONING = "provisioning"
CONF_NET_KEY = "net_key"
CONF_APP_KEY = "app_key"
CONF_UNICAST_ADDRESS = "unicast_address"
CONF_ELEMENTS = "elements"
CONF_MODELS = "models"

ROLE_NODE = "node"
ROLE_PROXY = "proxy"
ROLE_BOTH = "both"

ROLE_ENUM = {
    ROLE_NODE: ROLE_NODE,
    ROLE_PROXY: ROLE_PROXY,
    ROLE_BOTH: ROLE_BOTH,
}

MODEL_GENERIC_ONOFF_SERVER = "generic_onoff_server"
MODEL_GENERIC_LEVEL_SERVER = "generic_level_server"

MODEL_ENUM = {
    MODEL_GENERIC_ONOFF_SERVER: MODEL_GENERIC_ONOFF_SERVER,
    MODEL_GENERIC_LEVEL_SERVER: MODEL_GENERIC_LEVEL_SERVER,
}

ELEMENT_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_MODELS, default=[MODEL_GENERIC_ONOFF_SERVER]): cv.ensure_list(
            cv.enum(MODEL_ENUM)
        ),
    }
)

bluetooth_sig_mesh_ns = cg.esphome_ns.namespace("bluetooth_sig_mesh")
BluetoothSIGMesh = bluetooth_sig_mesh_ns.class_("BluetoothSIGMesh", cg.Component)
ESP32BluetoothSIGMesh = bluetooth_sig_mesh_ns.class_(
    "ESP32BluetoothSIGMesh", BluetoothSIGMesh
)
RP2040BluetoothSIGMesh = bluetooth_sig_mesh_ns.class_(
    "RP2040BluetoothSIGMesh", BluetoothSIGMesh
)
BK72XXBluetoothSIGMesh = bluetooth_sig_mesh_ns.class_(
    "BK72XXBluetoothSIGMesh", BluetoothSIGMesh
)
LN882HBluetoothSIGMesh = bluetooth_sig_mesh_ns.class_(
    "LN882HBluetoothSIGMesh", BluetoothSIGMesh
)


def AUTO_LOAD() -> list[str]:
    if CORE.is_esp32:
        return ["esp32_ble_tracker"]
    if CORE.is_rp2:
        return ["rp2040_ble"]
    if CORE.target_platform == PLATFORM_BK72XX:
        return ["bk72xx_ble"]
    if CORE.target_platform == PLATFORM_LN882X:
        return ["ln882h_ble"]
    return ["ble_device_base"]


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(BluetoothSIGMesh),
            cv.Optional(CONF_MESH_ROLE, default=ROLE_BOTH): cv.enum(ROLE_ENUM),
            cv.Optional(CONF_ENABLE_NODE, default=True): cv.boolean,
            cv.Optional(CONF_ENABLE_PROXY, default=True): cv.boolean,
            cv.Optional(CONF_NET_KEY): cv.string,
            cv.Optional(CONF_APP_KEY): cv.string,
            cv.Optional(CONF_UNICAST_ADDRESS): cv.hex_uint16_t,
            cv.Optional(CONF_ELEMENTS): cv.ensure_list(ELEMENT_SCHEMA),
        }
    ).extend(cv.COMPONENT_SCHEMA),
)


async def to_code(config: ConfigType) -> None:
    if CORE.is_esp32:
        klass = ESP32BluetoothSIGMesh
    elif CORE.is_rp2:
        klass = RP2040BluetoothSIGMesh
    elif CORE.target_platform == PLATFORM_BK72XX:
        klass = BK72XXBluetoothSIGMesh
    elif CORE.target_platform == PLATFORM_LN882X:
        klass = LN882HBluetoothSIGMesh
    else:
        klass = BluetoothSIGMesh

    var = cg.new_Pvariable(config[CONF_ID], klass())
    await cg.register_component(var, config)

    role = config[CONF_MESH_ROLE]
    enable_node = config[CONF_ENABLE_NODE] and role in (ROLE_NODE, ROLE_BOTH)
    enable_proxy = config[CONF_ENABLE_PROXY] and role in (ROLE_PROXY, ROLE_BOTH)

    cg.add(var.set_enable_node(enable_node))
    cg.add(var.set_enable_proxy(enable_proxy))

    if CONF_NET_KEY in config:
        cg.add(var.set_net_key(config[CONF_NET_KEY]))
    if CONF_APP_KEY in config:
        cg.add(var.set_app_key(config[CONF_APP_KEY]))
    if CONF_UNICAST_ADDRESS in config:
        cg.add(var.set_unicast_address(config[CONF_UNICAST_ADDRESS]))

    cg.add_define("USE_BLUETOOTH_SIG_MESH")
