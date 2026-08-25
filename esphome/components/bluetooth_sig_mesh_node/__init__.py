import esphome.codegen as cg
from esphome.components.bluetooth_sig_mesh import (
    CONF_BLUETOOTH_SIG_MESH_ID,
    CONF_DEVICE_KEY,
    CONF_UNICAST_ADDRESS,
    BluetoothSIGMesh,
    bluetooth_sig_mesh_ns,
    validate_hex_key_128,
    validate_unicast_address,
)
import esphome.config_validation as cv
from esphome.const import CONF_ID
from esphome.types import ConfigType

CODEOWNERS = ["@esphome"]

DEPENDENCIES = ["bluetooth_sig_mesh"]

BluetoothSIGMeshNode = bluetooth_sig_mesh_ns.class_(
    "BluetoothSIGMeshNode", cg.Component
)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_ID): cv.declare_id(BluetoothSIGMeshNode),
        cv.GenerateID(CONF_BLUETOOTH_SIG_MESH_ID): cv.use_id(BluetoothSIGMesh),
        cv.Required(CONF_UNICAST_ADDRESS): validate_unicast_address,
        cv.Optional(CONF_DEVICE_KEY): validate_hex_key_128,
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config: ConfigType) -> None:
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    parent = await cg.get_variable(config[CONF_BLUETOOTH_SIG_MESH_ID])
    cg.add(var.set_parent(parent))
    cg.add(var.set_unicast_address(config[CONF_UNICAST_ADDRESS]))

    if CONF_DEVICE_KEY in config:
        cg.add(var.set_device_key(config[CONF_DEVICE_KEY]))
