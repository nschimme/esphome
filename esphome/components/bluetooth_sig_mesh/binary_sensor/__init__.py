import esphome.codegen as cg
from esphome.components import binary_sensor
import esphome.config_validation as cv
from esphome.const import CONF_ID
from esphome.types import ConfigType

from .. import CLIENT_ENTITY_BASE_SCHEMA, CLIENT_ENTITY_SCHEMA, bluetooth_sig_mesh_ns, register_client_entity

CODEOWNERS = ["@esphome"]

BluetoothSIGMeshBinarySensor = bluetooth_sig_mesh_ns.class_(
    "BluetoothSIGMeshBinarySensor", binary_sensor.BinarySensor, cg.Component
)

CONFIG_SCHEMA = CLIENT_ENTITY_SCHEMA(binary_sensor.binary_sensor_schema(BluetoothSIGMeshBinarySensor))


async def to_code(config: ConfigType) -> None:
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await binary_sensor.register_binary_sensor(var, config)
    await register_client_entity(var, config)
