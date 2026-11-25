## 1. Core: event discovery builder (`src/core/discovery.{h,cpp}`)

- [x] 1.1 Write failing host tests in `tests/test_misc.cpp`: `DiscoveryEventTopicIsBuiltFromDeviceID` (`homeassistant/event/<device_id>/received/config`) and `DiscoveryEventPayloadContainsSpecifiedFields` (name, unique_id, state_topic, event_types, value_template, device_class, availability_topic, icon, device object)
- [x] 1.2 Extract the shared `device` object and `availability_topic` into a helper in `discovery.cpp`; `make test` still passes for the sensor tests
- [x] 1.3 Add `DiscoveryBuildEventTopic()` and `DiscoveryBuildEventPayload()` (declarations with doc comments in `discovery.h`); the tests from 1.1 pass
- [x] 1.4 Add tests `DiscoveryEventPayloadIsValidJSONWithEscapedTopicBase` (parse with the JSON tokenizer, check the template's quotes are escaped) and `DiscoveryEventPayloadFitsWorstCase` (31-char `device_id`, 63-char `topic_base` full of characters that need escaping, longest board name and version, < `DISCOVERY_PAYLOAD_MAX`)
- [x] 1.5 Add test `DiscoveryDeviceObjectIsIdenticalInBothPayloads` (extract the `"device":{…}` substring from both payloads and compare)

## 2. Firmware: publish the event config (`src/fw/irmqttclient.{h,cpp}`)

- [x] 2.1 Add `m_TopicDiscoveryEvent[TOPIC_MAX]`, build it in the constructor with `DiscoveryBuildEventTopic()`
- [x] 2.2 In `OnConnect()`, publish the event config retained after the sensor config, reusing the payload buffer; log "Event discovery payload too large" on overflow without skipping the sensor config
- [x] 2.3 `make` for all targets (no warnings) and `make test`

## 3. Hardware check

Moved to the change `verify-ir-blaster-hardware` (group 11, requirement "Home Assistant event entity"), so that all hardware tests live in one change. Host tests cover the discovery payloads (group 1).

## 4. Documentation

- [x] 4.1 README "Home Assistant" section: describe the event entity, when to use it vs. the sensor, and an automation example (`trigger: state` on `event.…_ir_event`, condition on `raw | length`)
- [x] 4.2 README: add the event discovery topic to the MQTT topic table, plus the rollback note (clear `homeassistant/event/<device_id>/received/config` with `mosquitto_pub -r -n`)
- [x] 4.3 Update `docs/dev/raspir-blaster-home-assistant.md` where it lists discovery messages (skip if it does not) — skipped: the design doc lists no discovery messages
- [x] 4.4 `CHANGELOG.md` `[Unreleased]` → Added: "IR Event" event entity in Home Assistant, which fires once per received IR frame, also for identical repeats
