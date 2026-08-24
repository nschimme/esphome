import esphome.codegen as cg
from esphome.components import cover
import esphome.config_validation as cv

from .. import CLIENT_ENTITY_SCHEMA, bluetooth_sig_mesh_ns, register_client_entity

DEPENDENCIES = ["bluetooth_sig_mesh"]
CODEOWNERS = ["@esphome"]

BluetoothSIGMeshCover = bluetooth_sig_mesh_ns.class_(
    "BluetoothSIGMeshCover", cover.Cover, cg.Component
)

CONFIG_SCHEMA = CLIENT_ENTITY_SCHEMA(cover.cover_schema(BluetoothSIGMeshCover))


async def to_code(config):
    var = await cover.new_cover(config)
    await cg.register_component(var, config)
    await register_client_entity(var, config)
