# ESPNow Component

The `espnow` component provides an interface to the ESP-Now protocol, which is a connectionless communication protocol developed by Espressif. It allows multiple devices to communicate with each other without the need for a Wi-Fi access point.

This component can be used to send and receive raw ESP-Now packets, and it can also be used as a `packet_transport` for other components, such as the `packet_transport` sensor platform.

## Configuration

```yaml
espnow:
  id: espnow_hub
  peers:
    - "AA:BB:CC:DD:EE:FF"
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
    use_broadcast: false
```

The `espnow` packet transport has the following sending modes:
-   **Default Peer:** If `default_peer` is set, all packets will be sent to this peer.
-   **Broadcast:** If `use_broadcast` is set to `true`, all packets will be broadcasted to all devices on the same channel.
-   **Multi-peer:** If neither `default_peer` nor `use_broadcast` is set, packets will be sent to all the peers that are configured in the parent `espnow` component.
-   If none of these options are configured, an error will be logged and no packets will be sent.

You can only use one of `default_peer` or `use_broadcast` at a time.

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
