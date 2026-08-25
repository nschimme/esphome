import esphome.codegen as cg
from esphome.components import switch

from .. import CLIENT_ENTITY_SCHEMA, bluetooth_sig_mesh_ns, register_client_entity

DEPENDENCIES = ["bluetooth_sig_mesh"]
CODEOWNERS = ["@esphome"]

BluetoothSIGMeshSwitch = bluetooth_sig_mesh_ns.class_(
    "BluetoothSIGMeshSwitch", switch.Switch, cg.Component
)

CONFIG_SCHEMA = CLIENT_ENTITY_SCHEMA(switch.switch_schema(BluetoothSIGMeshSwitch))


async def to_code(config):
    var = cg.new_Pvariable(config[switch.CONF_ID])
    await cg.register_component(var, config)
    await register_client_entity(var, config)
    await switch.register_switch(var, config)
