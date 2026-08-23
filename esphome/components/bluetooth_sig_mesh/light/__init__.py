import esphome.codegen as cg
from esphome.components import light
import esphome.config_validation as cv
from esphome.const import CONF_OUTPUT_ID

CONF_DST_ADDRESS = "dst_address"
from esphome.types import ConfigType

from .. import BluetoothSIGMesh, bluetooth_sig_mesh_ns

DEPENDENCIES = ["bluetooth_sig_mesh"]

BluetoothSIGMeshLight = bluetooth_sig_mesh_ns.class_(
    "BluetoothSIGMeshLight", light.LightOutput, cg.Component
)

CONFIG_SCHEMA = light.BRIGHTNESS_ONLY_LIGHT_SCHEMA.extend(
    {
        cv.GenerateID(CONF_OUTPUT_ID): cv.declare_id(BluetoothSIGMeshLight),
        cv.GenerateID(CONF_DST_ADDRESS): cv.hex_uint16_t,
        cv.GenerateID("mesh_id"): cv.use_id(BluetoothSIGMesh),
    }
)


async def to_code(config: ConfigType) -> None:
    var = cg.new_Pvariable(config[CONF_OUTPUT_ID])
    await cg.register_component(var, config)
    await light.register_light(var, config)

    parent = await cg.get_variable(config["mesh_id"])
    cg.add(var.set_parent(parent))
    cg.add(var.set_dst_address(config[CONF_DST_ADDRESS]))
