## MODIFIED Requirements

### Requirement: Supported keys and defaults
The firmware SHALL support the following keys:

| Key | Required | Default | Constraint |
| --- | --- | --- | --- |
| `mqtt_host` | yes | — | IPv4 address or host name, max 63 chars |
| `mqtt_port` | no | `1883` | 1–65535 |
| `mqtt_user` | no | none (anonymous) | max 63 chars |
| `mqtt_password` | no | none | max 63 chars; only used if `mqtt_user` is set |
| `device_id` | no | `pi-zero-ir` | 1–31 chars of `[a-z0-9-]`; also used as-is as the DHCP/mDNS host name, so `_` is not allowed |
| `topic_base` | no | `home/ir_blaster` | 1–63 chars, no `+`, `#` or trailing `/` |
| `ir_frame_gap_ms` | no | `15` | 5–60 |
| `ota_password` | no | none (updates from the web page disabled) | 8–63 chars |

#### Scenario: Only the required key
- **WHEN** the file contains only `mqtt_host`
- **THEN** all other settings take their defaults

#### Scenario: Out-of-range value
- **WHEN** an optional key has a value that violates its constraint (e.g. `ir_frame_gap_ms=200`)
- **THEN** the default is used for that key and a warning naming the key and the rejected value is logged

#### Scenario: device_id is used directly as the host name
- **WHEN** the configuration sets `device_id=living-room-ir`
- **THEN** the DHCP host name and the mDNS `.local` name are both `living-room-ir`, unchanged, because `device_id` only allows characters already valid in a host name

#### Scenario: Underscore rejected
- **WHEN** the configuration sets `device_id=living_room`
- **THEN** the key is rejected (invalid character `_`) and `device_id` falls back to `pi-zero-ir`

#### Scenario: Too short update password
- **WHEN** the file contains `ota_password=abc`
- **THEN** the key is ignored with a warning that does not contain the value, and updates from the web page stay disabled

### Requirement: Credentials are never logged
The firmware SHALL NOT write the value of `mqtt_password` or `ota_password` to any log output.

#### Scenario: Logging the configuration
- **WHEN** the firmware logs the effective configuration at boot
- **THEN** `mqtt_password` appears as `***` if set and is otherwise omitted, and `ota_password` likewise

### Requirement: Missing or unusable configuration
If the configuration file is missing, unreadable, or lacks a valid `mqtt_host`, the firmware SHALL log an error and SHALL NOT attempt MQTT connections. On boards with a Wi-Fi chip it SHALL enter setup mode (see web-update, "Setup mode"). On a board without a Wi-Fi chip (CM4 without wireless), and after setup mode has timed out, it SHALL keep running in the degraded state: keep feeding the watchdog so it does not enter a reboot loop, and indicate the error state by blinking the ACT LED.

#### Scenario: File missing
- **WHEN** the SD card has no `raspir-blaster.conf`
- **THEN** the firmware logs a "configuration missing" error, does not connect to MQTT, does not reboot, blinks the ACT LED in the error pattern, and opens the setup access point if it has a Wi-Fi chip

#### Scenario: Required key missing
- **WHEN** the file exists but contains no valid `mqtt_host`
- **THEN** the firmware behaves as if the file were missing and logs which key is missing

### Requirement: Wi-Fi credentials file
The firmware SHALL read Wi-Fi credentials from `wpa_supplicant.conf` in the SD card root, in the format Circle's WPA supplicant expects. The file SHALL only be required on boards without Ethernet. On boards with Ethernet, a missing file means the firmware uses Ethernet (see connectivity, "Network interface selection"). The file can also be written by the configuration endpoint in setup mode (see web-update).

#### Scenario: Wi-Fi file missing
- **WHEN** `wpa_supplicant.conf` is missing on a Zero W, Zero 2 W or Pi 3 A+
- **THEN** the firmware logs an error, does not reboot, blinks the ACT LED in the error pattern, and enters setup mode

#### Scenario: Wi-Fi file missing on a board with Ethernet
- **WHEN** `wpa_supplicant.conf` is missing on a Pi 3 B, Pi 3 B+, Pi 4 B or CM4
- **THEN** the firmware logs that it uses Ethernet because no Wi-Fi configuration exists, and does not enter the degraded state or setup mode
