import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components.packet_transport import PacketTransport
from esphome.const import CONF_ID

packet_transport_proxy_ns = cg.esphome_ns.namespace("packet_transport_proxy")
PacketTransportProxy = packet_transport_proxy_ns.class_("PacketTransportProxy", cg.Component)
ota_ns = cg.esphome_ns.namespace("ota")
PacketTransportProxyOTABackend = ota_ns.class_("PacketTransportProxyOTABackend")

CONF_PACKET_TRANSPORT_ID = "packet_transport_id"

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(PacketTransportProxy),
        cv.Required(CONF_PACKET_TRANSPORT_ID): cv.use_id(PacketTransport),
    }
).extend(cv.COMPONENT_SCHEMA)

async def to_code(config):
    var = cg.new_P(config[CONF_ID])
    await cg.register_component(var, config)
    cg.add_define("USE_PACKET_TRANSPORT_PROXY_OTA")
    backend = PacketTransportProxyOTABackend.get_instance()
    transport = await cg.get_variable(config[CONF_PACKET_TRANSPORT_ID])
    cg.add(backend.set_transport(transport))
