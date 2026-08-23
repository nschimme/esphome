from esphome import automation
from esphome.automation import Action
import esphome.codegen as cg
from esphome.components import ble_device_base, light, switch
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_LEVEL, CONF_LIGHT_ID, CONF_NAME, CONF_STATE
from esphome.core import CORE
from esphome.types import ConfigType

CODEOWNERS = ["@esphome"]

CONF_RELAY = "relay"
CONF_PROVISIONING = "provisioning"
CONF_NET_KEY = "net_key"
CONF_APP_KEY = "app_key"
CONF_UNICAST_ADDRESS = "unicast_address"
CONF_ADVERTISE_UNPROVISIONED = "advertise_unprovisioned"
CONF_REMOTE_NODES = "remote_nodes"
CONF_DEVICE_KEY = "device_key"
REMOTE_NODE_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_UNICAST_ADDRESS): cv.hex_uint16_t,
        cv.Required(CONF_DEVICE_KEY): cv.string,
        cv.Optional(CONF_NAME, default=""): cv.string,
    }
)

bluetooth_sig_mesh_ns = cg.esphome_ns.namespace("bluetooth_sig_mesh")
BluetoothSIGMesh = bluetooth_sig_mesh_ns.class_(
    "BluetoothSIGMesh", cg.Component, ble_device_base.ESPBTDeviceListener
)

SendOnOffAction = bluetooth_sig_mesh_ns.class_("SendOnOffAction", Action)
SendLevelAction = bluetooth_sig_mesh_ns.class_("SendLevelAction", Action)
SendLightnessAction = bluetooth_sig_mesh_ns.class_("SendLightnessAction", Action)

CONF_LIGHTNESS = "lightness"

SEND_ONOFF_ACTION_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.use_id(BluetoothSIGMesh),
        cv.Required(CONF_UNICAST_ADDRESS): cv.templatable(cv.hex_uint16_t),
        cv.Required(CONF_STATE): cv.templatable(cv.boolean),
    }
)

SEND_LEVEL_ACTION_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.use_id(BluetoothSIGMesh),
        cv.Required(CONF_UNICAST_ADDRESS): cv.templatable(cv.hex_uint16_t),
        cv.Required(CONF_LEVEL): cv.templatable(cv.int_range(-32768, 32767)),
    }
)

SEND_LIGHTNESS_ACTION_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.use_id(BluetoothSIGMesh),
        cv.Required(CONF_UNICAST_ADDRESS): cv.templatable(cv.hex_uint16_t),
        cv.Required(CONF_LIGHTNESS): cv.templatable(cv.int_range(0, 65535)),
    }
)


@automation.register_action(
    "bluetooth_sig_mesh.send_onoff",
    SendOnOffAction,
    SEND_ONOFF_ACTION_SCHEMA,
    synchronous=True,
)
async def send_onoff_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    parent = await cg.get_variable(config[CONF_ID])
    cg.add(var.set_parent(parent))
    template_ = await cg.templatable(config[CONF_UNICAST_ADDRESS], args, cg.uint16)
    cg.add(var.set_dst_address(template_))
    template_ = await cg.templatable(config[CONF_STATE], args, cg.bool_)
    cg.add(var.set_state(template_))
    return var


@automation.register_action(
    "bluetooth_sig_mesh.send_level",
    SendLevelAction,
    SEND_LEVEL_ACTION_SCHEMA,
    synchronous=True,
)
async def send_level_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    parent = await cg.get_variable(config[CONF_ID])
    cg.add(var.set_parent(parent))
    template_ = await cg.templatable(config[CONF_UNICAST_ADDRESS], args, cg.uint16)
    cg.add(var.set_dst_address(template_))
    template_ = await cg.templatable(config[CONF_LEVEL], args, cg.int16)
    cg.add(var.set_level(template_))
    return var


@automation.register_action(
    "bluetooth_sig_mesh.send_lightness",
    SendLightnessAction,
    SEND_LIGHTNESS_ACTION_SCHEMA,
    synchronous=True,
)
async def send_lightness_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    parent = await cg.get_variable(config[CONF_ID])
    cg.add(var.set_parent(parent))
    template_ = await cg.templatable(config[CONF_UNICAST_ADDRESS], args, cg.uint16)
    cg.add(var.set_dst_address(template_))
    template_ = await cg.templatable(config[CONF_LIGHTNESS], args, cg.uint16)
    cg.add(var.set_lightness(template_))
    return var
ESP32BluetoothSIGMesh = bluetooth_sig_mesh_ns.class_(
    "ESP32BluetoothSIGMesh", BluetoothSIGMesh
)
RP2040BluetoothSIGMesh = bluetooth_sig_mesh_ns.class_(
    "RP2040BluetoothSIGMesh", BluetoothSIGMesh
)


def AUTO_LOAD() -> list[str]:
    if CORE.is_esp32:
        return ["esp32_ble_tracker", "esp32_ble_server", "ble_device_base"]
    if CORE.is_rp2:
        return ["rp2040_ble", "ble_device_base"]
    return ["ble_device_base"]


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(BluetoothSIGMesh),
            cv.Optional(CONF_RELAY, default=True): cv.boolean,
            cv.Optional(CONF_NET_KEY): cv.string,
            cv.Optional(CONF_APP_KEY): cv.string,
            cv.Optional(CONF_UNICAST_ADDRESS): cv.hex_uint16_t,
            cv.Optional(CONF_ADVERTISE_UNPROVISIONED, default=False): cv.boolean,
            cv.Optional(CONF_REMOTE_NODES): cv.ensure_list(REMOTE_NODE_SCHEMA),
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
    else:
        klass = BluetoothSIGMesh

    var = cg.new_Pvariable(config[CONF_ID], klass())
    await cg.register_component(var, config)
    await ble_device_base.register_ble_device(var, config)

    cg.add(var.set_relay(config[CONF_RELAY]))

    if CONF_NET_KEY in config:
        cg.add(var.set_net_key(config[CONF_NET_KEY]))
    if CONF_APP_KEY in config:
        cg.add(var.set_app_key(config[CONF_APP_KEY]))
    if CONF_UNICAST_ADDRESS in config:
        cg.add(var.set_unicast_address(config[CONF_UNICAST_ADDRESS]))

    cg.add(var.set_advertise_unprovisioned(config[CONF_ADVERTISE_UNPROVISIONED]))

    if CONF_REMOTE_NODES in config:
        for node_conf in config[CONF_REMOTE_NODES]:
            cg.add(
                var.add_remote_node(
                    node_conf[CONF_UNICAST_ADDRESS],
                    node_conf[CONF_DEVICE_KEY],
                    node_conf[CONF_NAME],
                )
            )

    cg.add_define("USE_BLUETOOTH_SIG_MESH")
    cg.add_global(bluetooth_sig_mesh_ns.using)
