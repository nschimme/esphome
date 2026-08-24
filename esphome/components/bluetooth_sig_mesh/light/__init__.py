import esphome.codegen as cg
from esphome.components import light
import esphome.config_validation as cv
from esphome.const import CONF_COLOR_TEMPERATURE, CONF_NAME, CONF_OUTPUT_ID
from esphome.types import ConfigType

from .. import CLIENT_ENTITY_BASE_SCHEMA, CLIENT_ENTITY_SCHEMA, bluetooth_sig_mesh_ns, register_client_entity

DEPENDENCIES = ["bluetooth_sig_mesh"]

BluetoothSIGMeshLight = bluetooth_sig_mesh_ns.class_(
    "BluetoothSIGMeshLight", light.LightOutput, cg.Component
)

CONFIG_SCHEMA = CLIENT_ENTITY_SCHEMA(
    light.BRIGHTNESS_ONLY_LIGHT_SCHEMA.extend(
        {
            cv.GenerateID(CONF_OUTPUT_ID): cv.declare_id(BluetoothSIGMeshLight),
            cv.Optional(CONF_COLOR_TEMPERATURE, default=False): cv.boolean,
        }
    )
)


async def to_code(config: ConfigType) -> None:
    await light.new_light(config)
    var = await cg.get_variable(config[CONF_OUTPUT_ID])
    await register_client_entity(var, config)
    cg.add(var.set_color_temperature(config[CONF_COLOR_TEMPERATURE]))
