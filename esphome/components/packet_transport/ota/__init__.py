import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import ota
from esphome.components.packet_transport import packet_transport_ns
from esphome.const import CONF_ID

DEPENDENCIES = ["packet_transport"]

PacketTransportOTAComponent = packet_transport_ns.class_("PacketTransportOTAComponent", ota.OTAComponent)

CONFIG_SCHEMA = (
    ota.BASE_OTA_SCHEMA.extend(
        {
            cv.GenerateID(): cv.declare_id(PacketTransportOTAComponent),
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
)

async def to_code(config):
    var = cg.new_P(config[CONF_ID])
    await cg.register_component(var, config)
    await ota.ota_to_code(var, config)
