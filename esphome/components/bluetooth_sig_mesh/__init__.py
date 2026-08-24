from esphome import automation
from esphome.automation import Action
import esphome.codegen as cg
from esphome.components import ble_device_base, light, sensor, switch
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_LEVEL, CONF_LIGHT_ID, CONF_NAME, CONF_SENSOR_ID, CONF_STATE
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
CONF_BLUETOOTH_SIG_MESH_ID = "bluetooth_sig_mesh_id"
CONF_NODE_ID = "node_id"
CONF_ELEMENTS = "elements"
CONF_SWITCH_ID = "switch_id"
CONF_BEACON_INTERVAL = "beacon_interval"
CONF_GATT = "gatt"

ELEMENT_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_SWITCH_ID): cv.use_id(switch.Switch),
        cv.Optional(CONF_LIGHT_ID): cv.use_id(light.LightState),
        cv.Optional(CONF_SENSOR_ID): cv.use_id(sensor.Sensor),
    }
)

# Unicast addresses must be in range 0x0001..0x7FFF per Bluetooth SIG Mesh Spec v1.0.1 Section 3.4.2.4
def validate_unicast_address(value):
    val = cv.hex_uint16_t(value)
    if val < 0x0001 or val > 0x7FFF:
        raise cv.Invalid(
            f"Unicast address 0x{val:04X} is invalid. Must be in range 0x0001..0x7FFF per Bluetooth SIG Mesh Specification."
        )
    return val

def validate_hex_key_128(value):
    val = cv.string_strict(value)
    val_clean = val.replace(":", "").replace("-", "").replace(" ", "")
    if len(val_clean) != 32:
        raise cv.Invalid("Key must be a 128-bit hex string (32 hex characters)")
    try:
        int(val_clean, 16)
    except ValueError:
        raise cv.Invalid("Key contains invalid hex characters")
    return val_clean

bluetooth_sig_mesh_ns = cg.esphome_ns.namespace("bluetooth_sig_mesh")
BluetoothSIGMesh = bluetooth_sig_mesh_ns.class_(
    "BluetoothSIGMesh", cg.Component, ble_device_base.ESPBTDeviceListener
)

REMOTE_NODE_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_UNICAST_ADDRESS): validate_unicast_address,
        cv.Required(CONF_DEVICE_KEY): validate_hex_key_128,
        cv.Optional(CONF_NAME, default=""): cv.string,
    }
)

BluetoothSIGMeshNode = bluetooth_sig_mesh_ns.class_(
    "BluetoothSIGMeshNode", cg.Component
)

CLIENT_ENTITY_BASE_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_NODE_ID): cv.use_id(BluetoothSIGMeshNode),
        cv.GenerateID(CONF_BLUETOOTH_SIG_MESH_ID): cv.use_id(BluetoothSIGMesh),
        cv.Optional(CONF_UNICAST_ADDRESS): validate_unicast_address,
    }
).extend(cv.COMPONENT_SCHEMA)

def CLIENT_ENTITY_SCHEMA(schema):
    return cv.All(
        schema.extend(CLIENT_ENTITY_BASE_SCHEMA),
        cv.has_at_least_one_key(CONF_NODE_ID, CONF_UNICAST_ADDRESS),
    )


async def register_client_entity(var, config):
    if CONF_NODE_ID in config:
        node = await cg.get_variable(config[CONF_NODE_ID])
        cg.add(var.set_node(node))
    else:
        parent = await cg.get_variable(config[CONF_BLUETOOTH_SIG_MESH_ID])
        cg.add(var.set_parent(parent))
        cg.add(var.set_dst_address(config[CONF_UNICAST_ADDRESS]))

SendOnOffAction = bluetooth_sig_mesh_ns.class_("SendOnOffAction", Action)
SendLevelAction = bluetooth_sig_mesh_ns.class_("SendLevelAction", Action)
SendLightnessAction = bluetooth_sig_mesh_ns.class_("SendLightnessAction", Action)

CONF_LIGHTNESS = "lightness"

SEND_ONOFF_ACTION_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_ID): cv.use_id(BluetoothSIGMesh),
        cv.Required(CONF_UNICAST_ADDRESS): cv.templatable(cv.hex_uint16_t),
        cv.Required(CONF_STATE): cv.templatable(cv.boolean),
    }
)

SEND_LEVEL_ACTION_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_ID): cv.use_id(BluetoothSIGMesh),
        cv.Required(CONF_UNICAST_ADDRESS): cv.templatable(cv.hex_uint16_t),
        cv.Required(CONF_LEVEL): cv.templatable(cv.int_range(-32768, 32767)),
    }
)

SEND_LIGHTNESS_ACTION_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_ID): cv.use_id(BluetoothSIGMesh),
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
            cv.Optional(CONF_NET_KEY): validate_hex_key_128,
            cv.Optional(CONF_APP_KEY): validate_hex_key_128,
            cv.Optional(CONF_UNICAST_ADDRESS): validate_unicast_address,
            cv.Optional(CONF_ADVERTISE_UNPROVISIONED, default=False): cv.boolean,
            cv.Optional(CONF_BEACON_INTERVAL, default="0s"): cv.positive_time_period_milliseconds,
            cv.Optional(CONF_GATT, default=False): cv.boolean,
            cv.Optional(CONF_REMOTE_NODES): cv.ensure_list(REMOTE_NODE_SCHEMA),
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
    cg.add(var.set_beacon_interval(config[CONF_BEACON_INTERVAL]))
    cg.add(var.set_gatt_enabled(config[CONF_GATT]))

    if CONF_REMOTE_NODES in config:
        for node_conf in config[CONF_REMOTE_NODES]:
            cg.add(
                var.add_remote_node(
                    node_conf[CONF_UNICAST_ADDRESS],
                    node_conf[CONF_DEVICE_KEY],
                    node_conf[CONF_NAME],
                )
            )

    if CONF_ELEMENTS in config:
        for elem in config[CONF_ELEMENTS]:
            if CONF_SWITCH_ID in elem:
                sw = await cg.get_variable(elem[CONF_SWITCH_ID])
                cg.add(var.add_bound_switch(sw))
            if CONF_LIGHT_ID in elem:
                lgt = await cg.get_variable(elem[CONF_LIGHT_ID])
                cg.add(var.add_bound_light(lgt))
            if CONF_SENSOR_ID in elem:
                sens = await cg.get_variable(elem[CONF_SENSOR_ID])
                cg.add(var.add_bound_sensor(sens))
    else:
        # Auto-bind local switches and lights when elements block is omitted
        for sw_conf in CORE.config.get("switch", []):
            if sw_conf.get("platform") != "bluetooth_sig_mesh" and CONF_ID in sw_conf:
                sw = await cg.get_variable(sw_conf[CONF_ID])
                cg.add(var.add_bound_switch(sw))
        for lgt_conf in CORE.config.get("light", []):
            if lgt_conf.get("platform") != "bluetooth_sig_mesh" and CONF_ID in lgt_conf:
                lgt = await cg.get_variable(lgt_conf[CONF_ID])
                cg.add(var.add_bound_light(lgt))

    cg.add_define("USE_BLUETOOTH_SIG_MESH")
    cg.add_global(bluetooth_sig_mesh_ns.using)
