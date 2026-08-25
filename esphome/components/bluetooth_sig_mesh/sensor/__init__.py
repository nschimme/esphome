import esphome.codegen as cg
from esphome.components import sensor

from .. import CLIENT_ENTITY_SCHEMA, bluetooth_sig_mesh_ns, register_client_entity

DEPENDENCIES = ["bluetooth_sig_mesh"]
CODEOWNERS = ["@esphome"]

BluetoothSIGMeshSensor = bluetooth_sig_mesh_ns.class_(
    "BluetoothSIGMeshSensor", sensor.Sensor, cg.Component
)

CONFIG_SCHEMA = CLIENT_ENTITY_SCHEMA(sensor.sensor_schema(BluetoothSIGMeshSensor))


async def to_code(config):
    var = cg.new_Pvariable(config[sensor.CONF_ID])
    await cg.register_component(var, config)
    await register_client_entity(var, config)
    await sensor.register_sensor(var, config)
