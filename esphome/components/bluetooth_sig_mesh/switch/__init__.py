import esphome.codegen as cg
from esphome.components import switch
import esphome.config_validation as cv
from esphome.const import CONF_ID
from esphome.types import ConfigType

from .. import BluetoothSIGMesh, bluetooth_sig_mesh_ns

CONF_DST_ADDRESS = "dst_address"

DEPENDENCIES = ["bluetooth_sig_mesh"]

BluetoothSIGMeshSwitch = bluetooth_sig_mesh_ns.class_(
    "BluetoothSIGMeshSwitch", switch.Switch, cg.Component
)

CONFIG_SCHEMA = switch.switch_schema(BluetoothSIGMeshSwitch).extend(
    {
        cv.GenerateID(CONF_ID): cv.declare_id(BluetoothSIGMeshSwitch),
        cv.Required(CONF_DST_ADDRESS): cv.hex_uint16_t,
        cv.GenerateID("mesh_id"): cv.use_id(BluetoothSIGMesh),
    }
)


async def to_code(config: ConfigType) -> None:
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await switch.register_switch(var, config)

    parent = await cg.get_variable(config["mesh_id"])
    cg.add(var.set_parent(parent))
    cg.add(var.set_dst_address(config[CONF_DST_ADDRESS]))
