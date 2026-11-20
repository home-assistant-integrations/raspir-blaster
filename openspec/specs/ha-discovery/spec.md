# ha-discovery Specification

## Purpose
TBD - created by archiving change add-ir-blaster-firmware. Update Purpose after archive.
## Requirements
### Requirement: Discovery of the receiver sensor
After every successful MQTT connection, the firmware SHALL publish, retained, a Home Assistant MQTT Discovery configuration for a sensor to `homeassistant/sensor/<device_id>/received/config`. The configuration SHALL contain:
- `name`: `IR Receiver`
- `unique_id`: `<device_id>_received`
- `state_topic` and `json_attributes_topic`: `<topic_base>/received`
- `value_template`: `{{ value_json.raw | length }}`
- `unit_of_measurement`: `pulses`
- `availability_topic`: `<topic_base>/status`
- `icon`: `mdi:remote`

#### Scenario: Device appears in Home Assistant
- **WHEN** the device connects to the broker used by Home Assistant with MQTT discovery enabled
- **THEN** Home Assistant shows a sensor "IR Receiver" whose state is the number of timings of the last received frame, with the `raw` and `freq` values available as attributes

#### Scenario: Home Assistant restart
- **WHEN** Home Assistant restarts while the device stays connected
- **THEN** the sensor reappears from the retained discovery message without action by the device

### Requirement: Device metadata
Every discovery configuration SHALL include a `device` object with `identifiers` `["<device_id>"]`, `name` `raspir-blaster (<device_id>)`, `model` `Raspberry Pi Zero W (BCM2835, Circle bare-metal)`, `manufacturer` `DIY` and `sw_version` set to the firmware version.

#### Scenario: Device grouping
- **WHEN** Home Assistant processes the discovery configuration
- **THEN** the sensor is grouped under one device whose model and firmware version match the running firmware

### Requirement: Availability
The device SHALL be reported as available while connected and as unavailable otherwise, using the retained `online` payload and the `offline` Last Will on `<topic_base>/status`. These are the default availability payloads, so the discovery configuration does not need `payload_available` or `payload_not_available`.

#### Scenario: Device powered off
- **WHEN** the device loses power while connected
- **THEN** after the broker's keep-alive timeout, Home Assistant shows the sensor as unavailable

#### Scenario: Device back online
- **WHEN** the device reconnects
- **THEN** Home Assistant shows the sensor as available again

### Requirement: Multiple devices do not collide
All discovery topics, unique IDs and device identifiers SHALL be derived from `device_id`, so that two devices with different `device_id` and `topic_base` values coexist on one broker.

#### Scenario: Two devices
- **WHEN** two devices are configured with `device_id` `living_ir` and `bedroom_ir` and different `topic_base` values
- **THEN** Home Assistant shows two separate devices, each with its own receiver sensor

