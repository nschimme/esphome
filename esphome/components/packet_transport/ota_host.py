import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import ota
from esphome.const import CONF_PLATFORM, CONF_ID
from . import PacketTransport, packet_transport_ns

PacketTransportHostOTABackend = packet_transport_ns.class_("PacketTransportHostOTABackend", ota.OTABackend)

CONFIG_SCHEMA = ota.OTA_PLATFORM_SCHEMA.extend(
    {
        cv.GenerateID(): cv.declare_id(PacketTransportHostOTABackend),
        cv.Required(CONF_PLATFORM): "packet_transport_host",
        cv.Required("packet_transport_id"): cv.use_id(PacketTransport),
        cv.Required("provider"): cv.string,
    }
)

async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await ota.register_ota_platform(var, config)
    transport = await cg.get_variable(config["packet_transport_id"])
    cg.add(var.set_parent(transport))
    cg.add(var.set_provider(config["provider"]))
