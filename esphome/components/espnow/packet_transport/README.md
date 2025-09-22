# ESPNow Packet Transport

The `espnow` packet transport allows you to create a hub that can receive data from other ESPNow devices and expose them as sensors in ESPHome. It uses the `packet_transport` component to handle the decoding of sensor data.

This transport is particularly useful for creating a central hub that collects data from multiple low-power sensor nodes.

## Configuration

**Note:** For a reliable hub, it is recommended to use an ESP32 with an Ethernet connection.

To use the ESPNow packet transport, you first need to set up the `espnow` component. Then, you can configure the `espnow` packet transport and link it to the `espnow` component.

```yaml
espnow:
  id: espnow_hub

packet_transport:
  - platform: espnow
    id: espnow_transport
    espnow_id: espnow_hub

binary_sensor:
  - platform: packet_transport
    transport_id: espnow_transport
    provider: "11:22:33:44:55:66" # MAC address of the remote sensor node
    remote_id: "door_sensor"
    name: "Front Door Sensor"

sensor:
  - platform: packet_transport
    transport_id: espnow_transport
    provider: "11:22:33:44:55:66" # MAC address of the remote sensor node
    remote_id: "living_room_temp"
    name: "Living Room Temperature"
    accuracy_decimals: 1
    unit_of_measurement: "°C"
```

In this example:
- We define an `espnow` component with the ID `espnow_hub`.
- We define a `packet_transport` of platform `espnow` with the ID `espnow_transport`.
- We link the transport to the `espnow` component using `espnow_id: espnow_hub`.
- We define a `binary_sensor` and a `sensor` that receive data from the transport.
- The `provider` is the MAC address of the remote sensor node.
- The `remote_id` is the unique ID of the sensor on the remote node.

## Remote Sensor Node Configuration

The remote sensor node needs to be configured to send data to the hub. Here is an example of a remote sensor node that sends temperature data to the hub:

```yaml
espnow:
  peers:
    - "AA:BB:CC:DD:EE:FF" # MAC address of the hub

packet_transport:
  - platform: espnow
    id: espnow_transport
    # ... other transport options like encryption ...

sensor:
  - platform: dht
    pin: D1
    temperature:
      name: "Living Room Temperature"
      id: living_room_temp
    humidity:
      name: "Living Room Humidity"
      id: living_room_humi

# Send the temperature value every 60 seconds
interval:
  - interval: 60s
    then:
      - component.update: living_room_temp
```
In this remote node configuration, the `packet_transport` will automatically send the updated sensor values to all peers configured in the `espnow` component. The `id` of the temperature sensor (`living_room_temp`) must match the `remote_id` configured on the hub for the corresponding sensor.
