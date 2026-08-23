from esphome.automation import Action
import esphome.codegen as cg
from esphome.components import ble_device_base, light, switch
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_LIGHT_ID, PLATFORM_BK72XX, PLATFORM_LN882X
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
CONF_SWITCH_ID = "switch_id"

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
        cv.Optional(CONF_SWITCH_ID): cv.use_id(switch.Switch),
        cv.Optional(CONF_LIGHT_ID): cv.use_id(light.LightState),
    }
)

bluetooth_sig_mesh_ns = cg.esphome_ns.namespace("bluetooth_sig_mesh")
BluetoothSIGMesh = bluetooth_sig_mesh_ns.class_(
    "BluetoothSIGMesh", cg.Component, ble_device_base.ESPBTDeviceListener
)

SendOnOffAction = bluetooth_sig_mesh_ns.class_("SendOnOffAction", Action)
SendLevelAction = bluetooth_sig_mesh_ns.class_("SendLevelAction", Action)
SendLightnessAction = bluetooth_sig_mesh_ns.class_("SendLightnessAction", Action)
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
        return ["esp32_ble_tracker", "esp32_ble_server", "ble_device_base"]
    if CORE.is_rp2:
        return ["rp2040_ble", "ble_device_base"]
    if CORE.target_platform == PLATFORM_BK72XX:
        return ["bk72xx_ble", "ble_device_base"]
    if CORE.target_platform == PLATFORM_LN882X:
        return ["ln882h_ble", "ble_device_base"]
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
    )
    .extend(ble_device_base.BLE_DEVICE_SCHEMA)
    .extend(cv.COMPONENT_SCHEMA),
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
    await ble_device_base.register_ble_device(var, config)

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

    if CONF_ELEMENTS in config:
        for elem_conf in config[CONF_ELEMENTS]:
            if CONF_SWITCH_ID in elem_conf:
                sw = await cg.get_variable(elem_conf[CONF_SWITCH_ID])
                cg.add(var.add_bound_switch(sw))
            if CONF_LIGHT_ID in elem_conf:
                lgt = await cg.get_variable(elem_conf[CONF_LIGHT_ID])
                cg.add(var.add_bound_light(lgt))

    cg.add_define("USE_BLUETOOTH_SIG_MESH")
