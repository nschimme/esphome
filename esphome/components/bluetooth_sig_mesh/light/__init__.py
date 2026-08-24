import esphome.codegen as cg
from esphome.components import light
import esphome.config_validation as cv
from esphome.const import CONF_COLOR_TEMPERATURE, CONF_NAME, CONF_OUTPUT_ID
from esphome.types import ConfigType

from .. import (
    CONF_BLUETOOTH_SIG_MESH_ID,
    CONF_NODE_ID,
    CONF_UNICAST_ADDRESS,
    BluetoothSIGMesh,
    bluetooth_sig_mesh_ns,
    validate_unicast_address,
)

DEPENDENCIES = ["bluetooth_sig_mesh"]

BluetoothSIGMeshNode = bluetooth_sig_mesh_ns.class_("BluetoothSIGMeshNode", cg.Component)

BluetoothSIGMeshLight = bluetooth_sig_mesh_ns.class_(
    "BluetoothSIGMeshLight", light.LightOutput, cg.Component
)

CONFIG_SCHEMA = cv.All(
    light.BRIGHTNESS_ONLY_LIGHT_SCHEMA.extend(
        {
            cv.GenerateID(CONF_OUTPUT_ID): cv.declare_id(BluetoothSIGMeshLight),
            cv.Optional(CONF_NODE_ID): cv.use_id(BluetoothSIGMeshNode),
            cv.GenerateID(CONF_BLUETOOTH_SIG_MESH_ID): cv.use_id(BluetoothSIGMesh),
            cv.Optional(CONF_UNICAST_ADDRESS): validate_unicast_address,
            cv.Optional(CONF_COLOR_TEMPERATURE, default=False): cv.boolean,
        }
    ),
    cv.has_at_least_one_key(CONF_NODE_ID, CONF_UNICAST_ADDRESS),
)


async def to_code(config: ConfigType) -> None:
    var = cg.new_Pvariable(config[CONF_OUTPUT_ID])
    await cg.register_component(var, config)

    # Register the LightOutput directly with ESPHome light component
    await light.register_light(var, config)

    if CONF_NODE_ID in config:
        node = await cg.get_variable(config[CONF_NODE_ID])
        cg.add(var.set_node(node))
    else:
        parent = await cg.get_variable(config[CONF_BLUETOOTH_SIG_MESH_ID])
        cg.add(var.set_parent(parent))
        cg.add(var.set_dst_address(config[CONF_UNICAST_ADDRESS]))

    cg.add(var.set_color_temperature(config[CONF_COLOR_TEMPERATURE]))
