import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import ota
from esphome.const import CONF_PLATFORM, CONF_ID
from . import PacketTransport, packet_transport_ns

PacketTransportOTABackend = packet_transport_ns.class_("PacketTransportOTABackend", ota.OTABackend)

CONFIG_SCHEMA = ota.OTA_PLATFORM_SCHEMA.extend(
    {
        cv.GenerateID(): cv.declare_id(PacketTransportOTABackend),
        cv.Required(CONF_PLATFORM): "packet_transport",
        cv.Required("packet_transport_id"): cv.use_id(PacketTransport),
    }
)

async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await ota.register_ota_platform(var, config)
    transport = await cg.get_variable(config["packet_transport_id"])
    cg.add(transport.set_ota_backend(var))
