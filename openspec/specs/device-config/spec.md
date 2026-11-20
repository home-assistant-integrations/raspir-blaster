# device-config Specification

## Purpose
TBD - created by archiving change add-ir-blaster-firmware. Update Purpose after archive.
## Requirements
### Requirement: Configuration file on the SD card
The firmware SHALL read its settings at boot from the file `raspir-blaster.conf` in the root of the SD card's FAT32 partition. The format SHALL be one `key=value` pair per line. Leading and trailing whitespace around keys and values SHALL be ignored, lines starting with `#` and empty lines SHALL be ignored, and lines may end with LF or CRLF.

#### Scenario: Valid file
- **WHEN** the file contains `mqtt_host = 192.168.1.10` and `mqtt_port=1883` on separate lines with CRLF endings
- **THEN** the firmware uses host `192.168.1.10` and port `1883`

#### Scenario: Comments and blank lines
- **WHEN** the file contains lines starting with `#` and empty lines between settings
- **THEN** those lines are ignored and the remaining settings are applied

#### Scenario: Unknown key
- **WHEN** the file contains a key that is not defined by this specification
- **THEN** the key is ignored, a warning naming the key is logged, and the other settings are applied

### Requirement: Supported keys and defaults
The firmware SHALL support the following keys:

| Key | Required | Default | Constraint |
| --- | --- | --- | --- |
| `mqtt_host` | yes | — | IPv4 address or host name, max 63 chars |
| `mqtt_port` | no | `1883` | 1–65535 |
| `mqtt_user` | no | none (anonymous) | max 63 chars |
| `mqtt_password` | no | none | max 63 chars; only used if `mqtt_user` is set |
| `device_id` | no | `pi_zero_ir` | 1–31 chars of `[a-z0-9_]` |
| `topic_base` | no | `home/ir_blaster` | 1–63 chars, no `+`, `#` or trailing `/` |
| `ir_frame_gap_ms` | no | `15` | 5–60 |

#### Scenario: Only the required key
- **WHEN** the file contains only `mqtt_host`
- **THEN** all other settings take their defaults

#### Scenario: Out-of-range value
- **WHEN** an optional key has a value that violates its constraint (e.g. `ir_frame_gap_ms=200`)
- **THEN** the default is used for that key and a warning naming the key and the rejected value is logged

### Requirement: Missing or unusable configuration
If the configuration file is missing, unreadable, or lacks a valid `mqtt_host`, the firmware SHALL log an error and SHALL NOT attempt MQTT connections. It SHALL keep running, keep feeding the watchdog so it does not enter a reboot loop, and indicate the error state by blinking the ACT LED.

#### Scenario: File missing
- **WHEN** the SD card has no `raspir-blaster.conf`
- **THEN** the firmware logs a "configuration missing" error, does not connect to MQTT, does not reboot, and blinks the ACT LED in the error pattern

#### Scenario: Required key missing
- **WHEN** the file exists but contains no valid `mqtt_host`
- **THEN** the firmware behaves as if the file were missing and logs which key is missing

### Requirement: Credentials are never logged
The firmware SHALL NOT write the value of `mqtt_password` to any log output.

#### Scenario: Logging the configuration
- **WHEN** the firmware logs the effective configuration at boot
- **THEN** `mqtt_password` appears as `***` if set and is otherwise omitted

### Requirement: Wi-Fi credentials file
The firmware SHALL read Wi-Fi credentials from `wpa_supplicant.conf` in the SD card root, in the format Circle's WPA supplicant expects.

#### Scenario: Wi-Fi file missing
- **WHEN** `wpa_supplicant.conf` is missing
- **THEN** the firmware logs an error, does not reboot, and blinks the ACT LED in the error pattern

