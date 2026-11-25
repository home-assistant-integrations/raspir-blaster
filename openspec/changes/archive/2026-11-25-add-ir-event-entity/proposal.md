## Why

Home Assistant only learns about received IR frames through the "IR Receiver" sensor, whose state is the timing count and whose attributes hold `raw` and `freq`. A sensor models a value, not an occurrence. When two consecutive frames have the same count and the same timings, Home Assistant treats the second as no change. It does not write a state change, so state-triggered automations miss the repeat. Remote presses are events, and Home Assistant has an entity type for them: MQTT `event` entities fire once per message, repeats included.

## What Changes

- The firmware publishes a second retained MQTT Discovery configuration, for an `event` entity "IR Event" on the same device, at `homeassistant/event/<device_id>/received/config`.
- The event entity listens on the existing `<topic_base>/received` topic. A `value_template` in the discovery config turns each frame into `{"event_type":"received","freq":…,"raw":[…]}`, so every received frame fires one `received` event with `freq` and `raw` as event attributes.
- Both discovery configurations are published after every successful MQTT connection, like the sensor's today.
- The "IR Receiver" sensor, the `received` payload format and the `send` topic stay unchanged. Existing automations keep working.
- README: document the event entity and an automation example triggered by it.

## Non-goals

- **Matching specific buttons.** The device does not compare frames against learned codes, decode protocols (NEC, RC5, Sony, …) or emit per-button event types. Matching with a tolerance stays in Home Assistant (templates or scripts).
- **Removing or changing the sensor.** Deprecating "IR Receiver" or adding `force_update` to it would be a separate change.
- **Changing the MQTT wire format.** No `event_type` field is added to `<topic_base>/received` payloads, and no new topic is created.
- **Measuring the carrier.** Received frames still report `freq` 38000.
- **Cleaning up stale discovery configs** after a `device_id` change. This stays as today: the user deletes the old device in Home Assistant.

## Capabilities

### New Capabilities

None.

### Modified Capabilities

- `ha-discovery`: adds a requirement for the discovery of the receive event entity. Updates the "Device metadata", "Availability" and "Multiple devices do not collide" requirements so that their scenarios cover both entities.

## Impact

- **Code:** `src/core/discovery.{h,cpp}` gets an event discovery topic and payload builder, sharing the device object with the sensor builder. `src/fw/irmqttclient.{h,cpp}` publishes the second config in `OnConnect()`.
- **Tests:** `tests/test_misc.cpp` gets fields, JSON validity and escaping tests for the event payload, plus a worst-case size test against `DISCOVERY_PAYLOAD_MAX`.
- **Docs:** `README.md` (Home Assistant section), `CHANGELOG.md` (`[Unreleased]` → Added), and the technical design doc if it lists discovery messages.
- **MQTT traffic:** one extra retained message per connection. No extra message per received frame.
- **Home Assistant:** a new entity `event.<device>_ir_event`, which needs HA 2023.8 or later (MQTT event platform). Older HA versions ignore the unknown component, and the sensor keeps working.
