import esphome.codegen as cg
from esphome.components import number
import esphome.config_validation as cv
from esphome.const import CONF_ID
from esphome.types import ConfigType

from .. import CLIENT_ENTITY_BASE_SCHEMA, CLIENT_ENTITY_SCHEMA, bluetooth_sig_mesh_ns, register_client_entity

CODEOWNERS = ["@esphome"]

BluetoothSIGMeshNumber = bluetooth_sig_mesh_ns.class_(
    "BluetoothSIGMeshNumber", number.Number, cg.Component
)

CONFIG_SCHEMA = CLIENT_ENTITY_SCHEMA(number.number_schema(BluetoothSIGMeshNumber))


async def to_code(config: ConfigType) -> None:
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await number.register_number(var, config, min_value=-32768, max_value=32767, step=1)
    await register_client_entity(var, config)
