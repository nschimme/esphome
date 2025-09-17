import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components.packet_transport import (
    PacketTransport,
    new_packet_transport,
    transport_schema,
)

from .. import espnow_ns, ESPNowComponent

CONF_ESPNOW_ID = "espnow_id"

ESPNowTransport = espnow_ns.class_("ESPNowTransport", PacketTransport)


CONFIG_SCHEMA = transport_schema(ESPNowTransport).extend(
    {
        cv.GenerateID(CONF_ESPNOW_ID): cv.use_id(ESPNowComponent),
    }
)


async def to_code(config):
    var, providers = await new_packet_transport(config)
    paren = await cg.get_variable(config[CONF_ESPNOW_ID])
    cg.add(var.set_parent(paren))
    await cg.register_parented(var, config[CONF_ESPNOW_ID])
