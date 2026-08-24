import esphome.codegen as cg
from esphome.components import binary_sensor
import esphome.config_validation as cv
from esphome.const import CONF_ID
from esphome.types import ConfigType

from .. import (
    CONF_BLUETOOTH_SIG_MESH_ID,
    CONF_NODE_ID,
    CONF_UNICAST_ADDRESS,
    BluetoothSIGMesh,
    bluetooth_sig_mesh_ns,
    validate_unicast_address,
)

CODEOWNERS = ["@esphome"]

BluetoothSIGMeshNode = bluetooth_sig_mesh_ns.class_("BluetoothSIGMeshNode", cg.Component)

BluetoothSIGMeshBinarySensor = bluetooth_sig_mesh_ns.class_(
    "BluetoothSIGMeshBinarySensor", binary_sensor.BinarySensor, cg.Component
)

CONFIG_SCHEMA = cv.All(
    binary_sensor.binary_sensor_schema(BluetoothSIGMeshBinarySensor).extend(
        {
            cv.GenerateID(CONF_ID): cv.declare_id(BluetoothSIGMeshBinarySensor),
            cv.Optional(CONF_NODE_ID): cv.use_id(BluetoothSIGMeshNode),
            cv.GenerateID(CONF_BLUETOOTH_SIG_MESH_ID): cv.use_id(BluetoothSIGMesh),
            cv.Optional(CONF_UNICAST_ADDRESS): validate_unicast_address,
        }
    ),
    cv.has_at_least_one_key(CONF_NODE_ID, CONF_UNICAST_ADDRESS),
)


async def to_code(config: ConfigType) -> None:
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await binary_sensor.register_binary_sensor(var, config)

    if CONF_NODE_ID in config:
        node = await cg.get_variable(config[CONF_NODE_ID])
        cg.add(var.set_node(node))
    else:
        parent = await cg.get_variable(config[CONF_BLUETOOTH_SIG_MESH_ID])
        cg.add(var.set_parent(parent))
        cg.add(var.set_dst_address(config[CONF_UNICAST_ADDRESS]))
