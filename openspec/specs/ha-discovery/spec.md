# ha-discovery Specification

## Purpose
The Home Assistant MQTT discovery messages the device publishes so that it, its IR receiver sensor and its IR event entity appear in Home Assistant automatically.
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

### Requirement: Discovery of the receive event
After every successful MQTT connection, the firmware SHALL publish, retained, a Home Assistant MQTT Discovery configuration for an event entity to `homeassistant/event/<device_id>/received/config`, in addition to the sensor configuration. The configuration SHALL contain:
- `name`: `IR Event`
- `unique_id`: `<device_id>_ir_event`
- `state_topic`: `<topic_base>/received`
- `event_types`: `["received"]`
- `value_template`: `{"event_type":"received","freq":{{ value_json.freq }},"raw":{{ value_json.raw | tojson }}}`
- `device_class`: `button`
- `availability_topic`: `<topic_base>/status`
- `icon`: `mdi:remote`

The `<topic_base>/received` payload format SHALL stay unchanged (see `ir-message-format`).

#### Scenario: Event entity appears in Home Assistant
- **WHEN** the device connects to the broker used by Home Assistant with MQTT discovery enabled
- **THEN** Home Assistant shows an event entity "IR Event" next to the "IR Receiver" sensor on the same device

#### Scenario: One event per received frame
- **WHEN** the receiver captures a frame and the device publishes it to `<topic_base>/received`
- **THEN** the event entity fires one event of type `received` whose attributes contain the frame's `freq` and `raw`

#### Scenario: Identical frames in a row
- **WHEN** the device publishes two frames with identical `freq` and `raw` one after the other
- **THEN** the event entity fires two events, while the sensor's state and attributes do not change

#### Scenario: Payload fits the discovery buffer
- **WHEN** `device_id` and `topic_base` have their maximum lengths and `topic_base` contains characters that need JSON escaping
- **THEN** the event discovery payload is valid JSON and fits into the discovery payload buffer

### Requirement: Device metadata
Every discovery configuration SHALL include a `device` object with `identifiers` `["<device_id>"]`, `name` `raspir-blaster (<device_id>)`, `model` `<board name> (Circle bare-metal)`, `manufacturer` `DIY` and `sw_version` set to the firmware version. `<board name>` is the board model detected at runtime (e.g. `Raspberry Pi Zero W`, `Raspberry Pi 4 Model B`, `Compute Module 4`). The `device` object SHALL be identical in the sensor and the event configuration.

#### Scenario: Device grouping
- **WHEN** Home Assistant processes the discovery configurations
- **THEN** the sensor and the event entity are grouped under one device whose model and firmware version match the running firmware

#### Scenario: Model reflects the board
- **WHEN** the firmware runs on a Raspberry Pi 3 Model B+
- **THEN** each discovery configuration contains `"model":"Raspberry Pi 3 Model B+ (Circle bare-metal)"`

### Requirement: Availability
The device SHALL be reported as available while connected and as unavailable otherwise, using the retained `online` payload and the `offline` Last Will on `<topic_base>/status`. These are the default availability payloads, so the discovery configurations do not need `payload_available` or `payload_not_available`.

#### Scenario: Device powered off
- **WHEN** the device loses power while connected
- **THEN** after the broker's keep-alive timeout, Home Assistant shows the sensor and the event entity as unavailable

#### Scenario: Device back online
- **WHEN** the device reconnects
- **THEN** Home Assistant shows the sensor and the event entity as available again

### Requirement: Multiple devices do not collide
All discovery topics, unique IDs and device identifiers SHALL be derived from `device_id`, so that two devices with different `device_id` and `topic_base` values coexist on one broker.

#### Scenario: Two devices
- **WHEN** two devices are configured with `device_id` `living_ir` and `bedroom_ir` and different `topic_base` values
- **THEN** Home Assistant shows two separate devices, each with its own receiver sensor and its own event entity, and each event entity fires only for frames from its own device

