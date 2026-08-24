import esphome.codegen as cg
from esphome.components import light
import esphome.config_validation as cv
from esphome.const import CONF_COLOR_TEMPERATURE, CONF_OUTPUT_ID
from esphome.types import ConfigType

from .. import BluetoothSIGMesh, bluetooth_sig_mesh_ns

CONF_DST_ADDRESS = "dst_address"

DEPENDENCIES = ["bluetooth_sig_mesh"]

BluetoothSIGMeshLight = bluetooth_sig_mesh_ns.class_(
    "BluetoothSIGMeshLight", light.LightOutput, cg.Component
)

CONFIG_SCHEMA = light.BRIGHTNESS_ONLY_LIGHT_SCHEMA.extend(
    {
        cv.GenerateID(CONF_OUTPUT_ID): cv.declare_id(BluetoothSIGMeshLight),
        cv.Required(CONF_DST_ADDRESS): cv.hex_uint16_t,
        cv.GenerateID("mesh_id"): cv.use_id(BluetoothSIGMesh),
        cv.Optional(CONF_COLOR_TEMPERATURE, default=False): cv.boolean,
    }
)


async def to_code(config: ConfigType) -> None:
    var = cg.new_Pvariable(config[CONF_OUTPUT_ID])
    await cg.register_component(var, config)
    await light.setup_light_core_(var, config)

    parent = await cg.get_variable(config["mesh_id"])
    cg.add(var.set_parent(parent))
    cg.add(var.set_dst_address(config[CONF_DST_ADDRESS]))
    cg.add(var.set_color_temperature(config[CONF_COLOR_TEMPERATURE]))
