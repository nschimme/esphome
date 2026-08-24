import esphome.codegen as cg
from esphome.components import button
import esphome.config_validation as cv
from esphome.const import CONF_ID
from esphome.types import ConfigType

from .. import CLIENT_ENTITY_BASE_SCHEMA, CLIENT_ENTITY_SCHEMA, bluetooth_sig_mesh_ns, register_client_entity

CODEOWNERS = ["@esphome"]

BluetoothSIGMeshButton = bluetooth_sig_mesh_ns.class_(
    "BluetoothSIGMeshButton", button.Button, cg.Component
)

CONFIG_SCHEMA = CLIENT_ENTITY_SCHEMA(button.button_schema(BluetoothSIGMeshButton))


async def to_code(config: ConfigType) -> None:
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await button.register_button(var, config)
    await register_client_entity(var, config)
