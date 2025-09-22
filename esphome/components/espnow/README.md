# ESPNow Component

The `espnow` component provides a low-level interface to the ESP-NOW protocol. It allows you to send and receive data directly between ESP devices without the need for a Wi-Fi network.

## Configuration

**Note:** For a reliable hub, it is recommended to use an ESP32 with an Ethernet connection.

```yaml
espnow:
  id: espnow_hub
  peers:
    - "11:22:33:44:55:66"
```

In this example, we define an `espnow` component with the ID `espnow_hub` and one peer with the MAC address `11:22:33:44:55:66`.

## ESPNow Packet Transport

For a more high-level way to exchange sensor data between devices, you can use the ESPNow Packet Transport. This component builds on top of the `espnow` component and provides a convenient way to create a hub that collects data from remote sensor nodes.

See the [ESPNow Packet Transport documentation](./packet_transport/README.md) for more information.
