import esphome.codegen as cg
from esphome.components import number

from .. import CLIENT_ENTITY_SCHEMA, bluetooth_sig_mesh_ns, register_client_entity

DEPENDENCIES = ["bluetooth_sig_mesh"]
CODEOWNERS = ["@esphome"]

BluetoothSIGMeshNumber = bluetooth_sig_mesh_ns.class_(
    "BluetoothSIGMeshNumber", number.Number, cg.Component
)

CONFIG_SCHEMA = CLIENT_ENTITY_SCHEMA(number.number_schema(BluetoothSIGMeshNumber))


async def to_code(config):
    var = cg.new_Pvariable(config[number.CONF_ID])
    await cg.register_component(var, config)
    await register_client_entity(var, config)
    await number.register_number(var, config, min_value=0.0, max_value=100.0, step=1.0)
