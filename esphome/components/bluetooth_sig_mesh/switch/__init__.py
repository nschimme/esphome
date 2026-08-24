import esphome.codegen as cg
from esphome.components import switch
import esphome.config_validation as cv
from esphome.const import CONF_ID
from esphome.types import ConfigType

from .. import CLIENT_ENTITY_BASE_SCHEMA, CLIENT_ENTITY_SCHEMA, bluetooth_sig_mesh_ns, register_client_entity

DEPENDENCIES = ["bluetooth_sig_mesh"]

BluetoothSIGMeshSwitch = bluetooth_sig_mesh_ns.class_(
    "BluetoothSIGMeshSwitch", switch.Switch, cg.Component
)

CONFIG_SCHEMA = CLIENT_ENTITY_SCHEMA(switch.switch_schema(BluetoothSIGMeshSwitch))


async def to_code(config: ConfigType) -> None:
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await switch.register_switch(var, config)
    await register_client_entity(var, config)
