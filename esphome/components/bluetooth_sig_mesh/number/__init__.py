import esphome.codegen as cg
from esphome.components import number
import esphome.config_validation as cv

from .. import CLIENT_ENTITY_SCHEMA, bluetooth_sig_mesh_ns, register_client_entity

DEPENDENCIES = ["bluetooth_sig_mesh"]
CODEOWNERS = ["@esphome"]

BluetoothSIGMeshNumber = bluetooth_sig_mesh_ns.class_(
    "BluetoothSIGMeshNumber", number.Number, cg.Component
)

CONFIG_SCHEMA = CLIENT_ENTITY_SCHEMA(number.number_schema(BluetoothSIGMeshNumber))


async def to_code(config):
    var = await number.new_number(config, min_value=-32768, max_value=32767, step=1)
    await cg.register_component(var, config)
    await register_client_entity(var, config)
