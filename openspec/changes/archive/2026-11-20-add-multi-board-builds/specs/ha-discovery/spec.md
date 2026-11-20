## MODIFIED Requirements

### Requirement: Device metadata
Every discovery configuration SHALL include a `device` object with `identifiers` `["<device_id>"]`, `name` `raspir-blaster (<device_id>)`, `model` `<board name> (Circle bare-metal)`, `manufacturer` `DIY` and `sw_version` set to the firmware version. `<board name>` is the board model detected at runtime (e.g. `Raspberry Pi Zero W`, `Raspberry Pi 4 Model B`, `Compute Module 4`).

#### Scenario: Device grouping
- **WHEN** Home Assistant processes the discovery configuration
- **THEN** the sensor is grouped under one device whose model and firmware version match the running firmware

#### Scenario: Model reflects the board
- **WHEN** the firmware runs on a Raspberry Pi 3 Model B+
- **THEN** the discovery configuration contains `"model":"Raspberry Pi 3 Model B+ (Circle bare-metal)"`
