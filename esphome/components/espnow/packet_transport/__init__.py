import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components.packet_transport import (
    PacketTransport,
    new_packet_transport,
    transport_schema,
)
from esphome.cpp_types import PollingComponent
from .. import ESPNowComponent, espnow_ns

CODEOWNERS = ["@jesserockz"]
DEPENDENCIES = ["espnow"]

ESPNowTransport = espnow_ns.class_("ESPNowTransport", PacketTransport, PollingComponent)

CONFIG_SCHEMA = transport_schema(ESPNowTransport).extend({
    cv.Required("espnow_id"): cv.use_id(ESPNowComponent),
})


async def to_code(config):
    var, _ = await new_packet_transport(config)
    parent = await cg.get_variable(config["espnow_id"])
    cg.add(var.set_parent(parent))
    cg.add(parent.register_received_handler(var))
