"""
The ESPNow packet transport component allows sending and receiving data over ESP-Now.

This component can be used with the `packet_transport` sensor platform to create
sensors that receive their data over ESP-Now.

Example configuration:

  espnow:
    id: espnow_hub

  packet_transport:
    - platform: espnow
      id: espnow_transport
      espnow_id: espnow_hub
      default_peer: "FF:FF:FF:FF:FF:FF"

  sensor:
    - platform: packet_transport
      name: "ESPNow Test Sensor"
      id: test_sensor
      internal: true
      transport_id: espnow_transport
      provider: "test_provider"
"""

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components.packet_transport import (
    PacketTransport,
    new_packet_transport,
    transport_schema,
)
from esphome.const import CONF_ADDRESS

from .. import espnow_ns, ESPNowComponent

CONF_ESPNOW_ID = "espnow_id"
CONF_DEFAULT_PEER = "default_peer"
CONF_USE_BROADCAST = "use_broadcast"

ESPNowTransport = espnow_ns.class_("ESPNowTransport", PacketTransport)


CONFIG_SCHEMA = cv.All(
    transport_schema(ESPNowTransport).extend(
        {
            cv.GenerateID(CONF_ESPNOW_ID): cv.use_id(ESPNowComponent),
            cv.Optional(CONF_DEFAULT_PEER): cv.mac_address,
            cv.Optional(CONF_USE_BROADCAST, default=False): cv.boolean,
        }
    ),
    cv.has_at_most_one_key(CONF_DEFAULT_PEER, CONF_USE_BROADCAST),
)


async def to_code(config):
    var, _ = await new_packet_transport(config)
    await cg.register_parented(var, config[CONF_ESPNOW_ID])
    if CONF_DEFAULT_PEER in config:
        cg.add(var.set_default_peer(config[CONF_DEFAULT_PEER].parts))
    if config[CONF_USE_BROADCAST]:
        cg.add(var.set_use_broadcast(True))
