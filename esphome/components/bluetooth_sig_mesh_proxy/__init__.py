import esphome.codegen as cg
from esphome.components import ble_device_base
import esphome.config_validation as cv
from esphome.const import CONF_ID
from esphome.types import ConfigType

from ..bluetooth_sig_mesh import BluetoothSIGMesh

DEPENDENCIES = ["bluetooth_sig_mesh"]
CODEOWNERS = ["@esphome"]

CONF_MESH_ID = "mesh_id"

bluetooth_sig_mesh_proxy_ns = cg.esphome_ns.namespace("bluetooth_sig_mesh_proxy")
BluetoothSIGMeshProxy = bluetooth_sig_mesh_proxy_ns.class_(
    "BluetoothSIGMeshProxy", cg.Component
)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(BluetoothSIGMeshProxy),
        cv.GenerateID(CONF_MESH_ID): cv.use_id(BluetoothSIGMesh),
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config: ConfigType) -> None:
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    mesh = await cg.get_variable(config[CONF_MESH_ID])
    cg.add(var.set_mesh_parent(mesh))

    if cg.CORE.is_esp32:
        cg.add_define("USE_BLUETOOTH_SIG_MESH_PROXY")
