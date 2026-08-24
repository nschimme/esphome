import esphome.codegen as cg
from esphome.components import light
import esphome.config_validation as cv
from esphome.const import CONF_COLOR_TEMPERATURE

from .. import CLIENT_ENTITY_SCHEMA, bluetooth_sig_mesh_ns, register_client_entity

DEPENDENCIES = ["bluetooth_sig_mesh"]
CODEOWNERS = ["@esphome"]

BluetoothSIGMeshLight = bluetooth_sig_mesh_ns.class_(
    "BluetoothSIGMeshLight", light.LightOutput, cg.Component
)

CONFIG_SCHEMA = CLIENT_ENTITY_SCHEMA(
    light.light_schema(BluetoothSIGMeshLight, light.LightType.BRIGHTNESS_ONLY).extend(
        {
            cv.Optional(CONF_COLOR_TEMPERATURE): cv.boolean,
        }
    )
)


async def to_code(config):
    var = cg.new_Pvariable(config[light.CONF_OUTPUT_ID])
    await cg.register_component(var, config)
    await register_client_entity(var, config)
    if CONF_COLOR_TEMPERATURE in config:
        cg.add(var.set_color_temperature(config[CONF_COLOR_TEMPERATURE]))
    await light.register_light(var, config)
