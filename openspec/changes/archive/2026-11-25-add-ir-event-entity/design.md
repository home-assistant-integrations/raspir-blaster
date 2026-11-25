## Context

The device announces itself to Home Assistant with one retained MQTT Discovery message, for the "IR Receiver" sensor (`src/core/discovery.cpp`, published in `CIRMqttClient::OnConnect()`). Each received frame is published, not retained and with QoS 1, to `<topic_base>/received` as `{"freq":38000,"raw":[…]}` (`ir-message-format`).

The sensor's state is `{{ value_json.raw | length }}` and its attributes are the JSON payload. Home Assistant drops a state write when neither the state nor the attributes changed, so two identical frames in a row produce one state change. MQTT `event` entities (HA 2023.8+) fire for every message. Their payload must be JSON with an `event_type` from the configured `event_types`, and any other keys become event attributes. A `value_template` may build that JSON from the raw payload.

Constraints: `src/core` may only include `<stdint.h>`/`<stddef.h>`, and discovery payloads are built into a `DISCOVERY_PAYLOAD_MAX` (1024 B) stack buffer with `CTextWriter`. Circle's `CMQTTClient` must publish with QoS ≥ 1.

## Goals / Non-Goals

**Goals:**
- One Home Assistant event per received frame, repeats included, with `freq` and `raw` as event attributes.
- No change to the wire format or the sensor, so existing automations and dashboards keep working.
- Builder logic in `src/core`, covered by host tests like the sensor builder.

**Non-Goals:**
- Button matching, protocol decoding, per-button event types.
- Removing the sensor, or adding `force_update` to it.
- Removing stale discovery configs after a `device_id` change.

## Decisions

### D1: Reuse `<topic_base>/received` and adapt it with a `value_template`

The event entity subscribes to the existing topic. Its `value_template` is `{"event_type":"received","freq":{{ value_json.freq }},"raw":{{ value_json.raw | tojson }}}`.

Alternatives:
- *Add `"event_type":"received"` to every `received` payload.* This is simpler for HA, but it changes `ir-message-format`, adds a meaningless `event_type` attribute to the sensor, and leaks HA semantics into the device protocol.
- *Publish frames a second time to a new `<topic_base>/event` topic.* This doubles the per-frame traffic and adds a second serialization of up to 4 KB in the main loop.

The template costs HA one render per frame, which is negligible even at 512 timings.

### D2: Single event type `received`

`event_types: ["received"]`. The device cannot tell buttons apart (no decoding, by design), so finer types would be invented. Automations filter on `trigger.to_state.attributes.raw`, or on a template derived from it.

### D3: Discovery topic, unique ID, name, device class

- **Topic:** `homeassistant/event/<device_id>/received/config`. The object ID `received` matches the sensor. Uniqueness comes from the component (`event` vs `sensor`).
- **Unique ID:** `<device_id>_ir_event`. It differs from the sensor's `<device_id>_received`, because unique IDs must be unique per platform, and a distinct ID avoids confusion in the entity registry.
- **Name:** `IR Event`. HA prefixes the device name, giving `event.raspir_blaster_<device_id>_ir_event`.
- **Device class:** `button`, the HA event device class for remote/button presses. It gives the right icon and logbook wording. `icon: mdi:remote` matches the sensor.

### D4: Shared builder for the `device` object and common fields

Move the `"device":{…}` object and `availability_topic` into a helper in `discovery.cpp`, and add `DiscoveryBuildEventTopic()` / `DiscoveryBuildEventPayload()` next to the existing functions. The existing `DiscoveryBuildTopic()` / `DiscoveryBuildPayload()` names stay (sensor), so the change to call sites stays small. One shared helper keeps the requirement "the `device` object SHALL be identical" true by construction.

The `value_template` contains double quotes and `{{ }}`. It is written with `AppendJSONString()` like every other string field, so the quotes are escaped and the payload stays valid JSON.

### D5: Publish both configs in `OnConnect()`, reusing one buffer

`OnConnect()` builds and publishes the sensor config, then the event config, into the same `char Payload[DISCOVERY_PAYLOAD_MAX]`. `PublishText()` copies into the MQTT send buffer before returning, so reusing the buffer is safe. If either build overflows, only that config is skipped and logged ("Discovery payload too large"), and the other is still published. A new `m_TopicDiscoveryEvent[TOPIC_MAX]` member holds the topic, built in the constructor like the existing one.

## Risks / Trade-offs

- **[Payload size]** The event payload is about 120 B longer than the sensor's because of the template. → A host test builds it with a 31-char `device_id`, a 63-char `topic_base` full of characters that need escaping, and the longest board name and version, and checks it fits into 1024 B. If it ever doesn't, the build returns 0 and the error is logged. It is never truncated.
- **[Old Home Assistant]** HA before 2023.8 has no MQTT event platform. → It logs and ignores the unknown component, and the sensor still works.
- **[Template drift]** If `ir-message-format` ever renames `raw` or `freq`, the template breaks silently in HA. → The host test asserts the template text, and the spec ties both together. Any format change has to touch this requirement.
- **[Two entities for one stream]** Users may wonder which to use. → The README says to use the event for automations and the sensor for the last captured frame, e.g. while learning a code.

## Migration Plan

No migration. After the firmware update, the next MQTT connection publishes the new retained config, and HA adds the event entity to the existing device. Rollback: flash the previous firmware, then delete the stale retained config with `mosquitto_pub -r -n -t homeassistant/event/<device_id>/received/config`. The README notes this.

## Open Questions

None.
