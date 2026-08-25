import esphome.codegen as cg
from esphome.components import button

from .. import CLIENT_ENTITY_SCHEMA, bluetooth_sig_mesh_ns, register_client_entity

DEPENDENCIES = ["bluetooth_sig_mesh"]
CODEOWNERS = ["@esphome"]

BluetoothSIGMeshButton = bluetooth_sig_mesh_ns.class_(
    "BluetoothSIGMeshButton", button.Button, cg.Component
)

CONFIG_SCHEMA = CLIENT_ENTITY_SCHEMA(button.button_schema(BluetoothSIGMeshButton))


async def to_code(config):
    var = cg.new_Pvariable(config[button.CONF_ID])
    await cg.register_component(var, config)
    await register_client_entity(var, config)
    await button.register_button(var, config)
