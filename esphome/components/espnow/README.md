# ESPNow Component

The `espnow` component provides an interface to the ESP-Now protocol, which is a connectionless communication protocol developed by Espressif. It allows multiple devices to communicate with each other without the need for a Wi-Fi access point.

This component can be used to send and receive raw ESP-Now packets, and it can also be used as a `packet_transport` for other components, such as the `packet_transport` sensor platform.

## Configuration

```yaml
espnow:
  id: espnow_hub
  # ... other espnow options
```

## Packet Transport

The `espnow` component can be used as a `packet_transport` to send and receive data for other components.

### Configuration

```yaml
packet_transport:
  - platform: espnow
    id: espnow_transport
    espnow_id: espnow_hub
    default_peer: "FF:FF:FF:FF:FF:FF"
```

The `default_peer` option is the MAC address of the peer to send packets to. If this option is not set, packets will not be sent.

### Example Usage

Here is an example of how to use the `espnow` packet transport with the `packet_transport` sensor platform:

```yaml
sensor:
  - platform: packet_transport
    name: "ESPNow Test Sensor"
    id: test_sensor
    internal: true
    transport_id: espnow_transport
    provider: "test_provider"
```

This will create a sensor that receives its data over ESP-Now. The data should be sent from another device using the `packet_transport` framework.
