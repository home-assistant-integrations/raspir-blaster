# web-update Specification

## Purpose
TBD - created by archiving change add-web-update. Update `openspec/specs/web-update/spec.md` with a more specific purpose.
## Requirements
### Requirement: HTTP API
Once its network is up (normal mode) or its access point is open (setup mode), the firmware SHALL serve an HTTP API on TCP port 80 under `/api/`. Requests SHALL carry parameters as `application/x-www-form-urlencoded` (at most 2 KB) or, for uploads, `multipart/form-data`. Every response the firmware itself decides on SHALL be a JSON object with `Content-Type: application/json`, HTTP status 200 and at least the field `"result"`. Because Circle's `CHTTPDaemon` always substitutes its own generic HTML page for any non-200 response and offers no hook to replace it, these errors SHALL also be answered with status 200 and SHALL be distinguished only by the JSON body: `"result":"error"`, a numeric `"status"` carrying the error's logical HTTP status (400 invalid request, 403 wrong/missing password or not allowed in this mode, 404 unknown path, 409 update already in progress) and a `"message"`. The one exception is a `POST /api/update` body over 8 MB: Circle's daemon rejects it before the firmware's own request handling runs, so that response is a real HTTP 413 with Circle's own HTML body, not JSON. Responses SHALL never contain the values of `mqtt_password`, `ota_password` or the Wi-Fi password.

#### Scenario: Unknown path
- **WHEN** a client requests `GET /`
- **THEN** the response has HTTP status 200 and a JSON body with `"result":"error"`, `"status":404`

### Requirement: Status endpoint
`GET /api/status` SHALL return `device_id`, `board`, `version`, `build` (`sdcard` or `cm4-emmc`), `mode` (`normal` or `setup`), `network` (`ethernet`, `wifi` or `access-point`), `ip`, `mqtt` (`connected` or `disconnected`), `uptime_s` and `update_enabled` (whether updates are allowed in the current mode). It SHALL need no password.

#### Scenario: Status in normal mode
- **WHEN** a client requests `GET /api/status` from a device connected to MQTT over Wi-Fi
- **THEN** the response has status 200 and contains `"mode":"normal"`, `"network":"wifi"` and `"mqtt":"connected"`

### Requirement: Password in normal mode
In normal mode, `POST /api/update`, `POST /api/update/confirm`, `POST /api/update/cancel`, `POST /api/restart` and `POST /api/config` SHALL require the form field `password` to equal `ota_password`. `GET /api/config` SHALL NOT accept a password in any form (see "Configuration endpoints") and SHALL answer `"status":403` in normal mode. Without `ota_password` in the configuration the password-protected endpoints SHALL answer `"status":403` with the message that updates and restarts are disabled. A wrong password SHALL be answered `"status":403` after a delay of 1 s, without looking at an uploaded file, and SHALL be logged without the entered value. In setup mode no password is required.

#### Scenario: Wrong password
- **WHEN** a zip is uploaded in normal mode with a wrong password
- **THEN** the response has `"result":"error"`, `"status":403`, nothing on the SD card changes, and the log contains a refusal without the entered password

#### Scenario: No ota_password configured
- **WHEN** `raspir-blaster.conf` has no `ota_password` and a client calls `POST /api/restart`
- **THEN** the response has `"status":403` and a message that updates and restarts are disabled

### Requirement: Update upload
`POST /api/update` SHALL accept the form fields `file` (the release zip, at most 8 MB) and, in normal mode, `password`. The firmware SHALL handle one update at a time; a second upload while one is processed SHALL be answered `"status":409`. A larger file SHALL be refused with real HTTP status 413 without changing anything (see the HTTP API requirement: this case is rejected by Circle's daemon before the firmware's handling runs, so the body is Circle's HTML, not JSON).

#### Scenario: File too large
- **WHEN** a file larger than 8 MB is uploaded
- **THEN** the response has HTTP status 413 and nothing on the SD card changes

### Requirement: Update validation
Before anything is written, the firmware SHALL check the uploaded zip and refuse it with `"status":400` and a message naming the reason, if:
- it is not a readable zip (only the methods "stored" and "deflate" are supported),
- it has no `raspir-blaster-manifest.txt`, or the manifest is malformed,
- the manifest's `build` (`sdcard` or `cm4-emmc`) differs from the running firmware's build,
- it does not contain the kernel image for the running board's build target (`kernel.img`, `kernel8.img` or `kernel8-rpi4.img`), or
- the SHA-256 of that extracted image differs from the manifest.

#### Scenario: eMMC zip on an SD card device
- **WHEN** `raspir-blaster-<version>-cm4-emmc.zip` is uploaded to a Pi 4 B
- **THEN** the response has `"status":400` and a message that the zip is for a CM4 with eMMC, and nothing changes

#### Scenario: Damaged zip
- **WHEN** a release zip with one changed byte inside the kernel image is uploaded
- **THEN** the response has `"status":400` because of the checksum, and nothing changes

### Requirement: Version warning
The firmware SHALL compare the manifest's `version` with its own version. Versions of the form `yyyy.mm.rm` SHALL be compared numerically field by field. If the uploaded version is newer, the firmware SHALL install it and answer `"result":"installed"`. Otherwise, or if either version is not of that form (e.g. `dev`), it SHALL answer `"result":"confirm_required"` with `installed_version`, `uploaded_version`, a `reason` (`same`, `older` or `not_comparable`) and a one-time `token`, and keep the extracted image in RAM for at most 10 minutes. `POST /api/update/confirm` with `token` (and, in normal mode, `password`) SHALL install it; an unknown or expired token SHALL be answered `"status":400`. `POST /api/update/cancel` (and, in normal mode, `password`) SHALL discard the pending image without installing it and answer `"result":"cancelled"`; with no update pending (already confirmed, cancelled or expired) it SHALL answer `"status":400`.

#### Scenario: Cancel a pending update
- **WHEN** a device running `2026.11.2` gets a valid zip with version `2026.11.2` and then calls `POST /api/update/cancel` with the token
- **THEN** the response is `"result":"cancelled"`, nothing is installed, and a later `POST /api/update/confirm` with that token answers `"status":400`

#### Scenario: Newer version
- **WHEN** a device running `2026.11.2` gets a valid zip with version `2026.12.0`
- **THEN** the response is `"result":"installed"` and the device restarts with the new version

#### Scenario: Same version
- **WHEN** a device running `2026.11.2` gets a valid zip with version `2026.11.2`
- **THEN** the response is `"result":"confirm_required"` with `"reason":"same"` and a token, and the update is installed only after `POST /api/update/confirm` with that token

#### Scenario: Older version
- **WHEN** a device running `2026.11.10` gets a valid zip with version `2026.11.9`
- **THEN** the response has `"reason":"older"` (numeric comparison, not text)

#### Scenario: Development build
- **WHEN** either the running firmware or the zip has version `dev`
- **THEN** the response has `"reason":"not_comparable"`

### Requirement: Installation
The firmware SHALL install only the kernel image of its own build target; all other files on the boot medium SHALL stay unchanged. It SHALL:
1. write the image as `<image>.new` and read it back, comparing its SHA-256 with the manifest,
2. delete an existing `<image>.bak`, rename the current `<image>` to `<image>.bak`, and rename `<image>.new` to `<image>`,
3. log the update (old and new version), send the response, and reboot about 2 s later.

If a step fails, the firmware SHALL remove `<image>.new`, keep or restore the current `<image>`, answer with an error, and keep running without a reboot.

#### Scenario: Successful update
- **WHEN** a valid newer zip is uploaded to a Zero W
- **THEN** `kernel.img` is replaced, the previous one is kept as `kernel.img.bak`, no other file on the card changes, and after the reboot `GET /api/status` and Home Assistant show the new version

#### Scenario: Write error
- **WHEN** writing `<image>.new` fails (e.g. the card is full)
- **THEN** the response is an error, `<image>` is unchanged, `<image>.new` does not exist, and the device keeps running

#### Scenario: Manual restore
- **WHEN** the new kernel does not start
- **THEN** renaming `<image>.bak` to `<image>` on the card in a computer restores the previous firmware (documented in the README)

### Requirement: Restart endpoint
`POST /api/restart` SHALL answer `"result":"restarting"` and reboot the device about 2 s later.

#### Scenario: Restart after setup
- **WHEN** a client in setup mode calls `POST /api/restart` after a successful `POST /api/config`
- **THEN** the device reboots and starts in normal mode with the new configuration

### Requirement: Setup mode
The firmware SHALL enter setup mode instead of the degraded state if it has a Wi-Fi chip and
- `raspir-blaster.conf` is missing, unreadable or has no valid `mqtt_host`, or
- the board has no Ethernet (Zero W, Zero 2 W, Pi 3 A+) and `wpa_supplicant.conf` is missing.

In setup mode the firmware SHALL:
- open an open (unencrypted) Wi-Fi access point with the SSID `raspir-blaster-<xxxx>`, where `<xxxx>` are the last four hex digits of its Wi-Fi MAC address in lower case, on channel 6, with the address 192.168.4.1/24,
- answer DHCP requests of clients with addresses from 192.168.4.10 to 192.168.4.50 (lease 1 hour, router and DNS server not set),
- serve the HTTP API without password, including the configuration endpoints,
- not connect to MQTT, log the SSID, and blink the ACT LED in the degraded pattern,
- close the access point after 30 minutes without an API request and then stay in the degraded state until it is powered off and on.

A CM4 without a Wi-Fi chip SHALL keep the degraded state.

#### Scenario: First start without configuration
- **WHEN** a Zero W boots from a card without `raspir-blaster.conf`
- **THEN** a phone sees the open network `raspir-blaster-<xxxx>`, gets an address from 192.168.4.10–50 after joining, and `GET http://192.168.4.1/api/status` answers `"mode":"setup"`

#### Scenario: Timeout
- **WHEN** no API request arrives for 30 minutes in setup mode
- **THEN** the access point disappears, the log says so, and the device stays in the degraded state until a power cycle

#### Scenario: Board without Wi-Fi chip
- **WHEN** a CM4 without wireless boots without `raspir-blaster.conf`
- **THEN** it enters the degraded state as before, without an access point

### Requirement: Configuration endpoints
`GET /api/config` SHALL be setup mode only (403 in normal mode) and SHALL require no password, because a GET request has no body and the firmware exposes no request headers — a password would have nowhere to go but the URL, which leaks into logs, browser history and proxies. `POST /api/config` SHALL be available in both modes: no password in setup mode; in normal mode it SHALL require the form field `password` to equal `ota_password`, under the same rules as [Password in normal mode](#requirement-password-in-normal-mode) (403 without `ota_password` configured, 403 after a 1 s delay on a wrong password). A `POST /api/config` request whose only field is `password` (no `raspir-blaster.conf` or Wi-Fi field) SHALL change nothing and instead answer as `GET /api/config` would — the normal-mode equivalent of reading the configuration.
- `GET /api/config` and a password-only `POST /api/config` SHALL return the current values of all `raspir-blaster.conf` keys and the Wi-Fi `ssid` and `country`, with `mqtt_password`, `ota_password` and the Wi-Fi password replaced by a flag whether they are set.
- `POST /api/config` with at least one `raspir-blaster.conf` or Wi-Fi field SHALL accept the `raspir-blaster.conf` keys and `wifi_ssid`, `wifi_password`, `wifi_country` (two letters). It SHALL validate them with the same rules as the configuration file (see device-config) and refuse the request with `"status":400` and a list of the invalid fields if any value is invalid. Otherwise it SHALL write `raspir-blaster.conf` (all given keys) and, if `wifi_ssid` is given, `wpa_supplicant.conf` (WPA2-PSK, or an open network if no password is given), each via a temporary file and rename, and answer `"result":"saved"`. The new configuration SHALL take effect after `POST /api/restart`.

#### Scenario: Configure a Zero W
- **WHEN** in setup mode a client posts `mqtt_host=192.168.1.10`, `wifi_ssid=Home`, `wifi_password=secret123`, `wifi_country=DE` and then calls `POST /api/restart`
- **THEN** the card contains a `raspir-blaster.conf` with that `mqtt_host` and a `wpa_supplicant.conf` for the network "Home", and after the restart the device joins "Home" and connects to MQTT

#### Scenario: Invalid value
- **WHEN** a client posts `mqtt_port=70000`
- **THEN** the response has `"status":400` listing `mqtt_port`, and no file is changed

#### Scenario: Secrets not returned
- **WHEN** a client requests `GET /api/config` after a password was saved
- **THEN** the response contains `"mqtt_password_set":true` but not the password

#### Scenario: Normal mode requires the password
- **WHEN** a client calls `POST /api/config` on a device in normal mode without `password`
- **THEN** the response has `"status":403` and no file is changed

#### Scenario: Reading in normal mode
- **WHEN** a client in normal mode calls `POST /api/config` with only `password` (correct) and no other field
- **THEN** the response is the current configuration (as `GET /api/config` returns in setup mode), and no file is changed

#### Scenario: GET never carries a password
- **WHEN** a client calls `GET /api/config` on a device in normal mode
- **THEN** the response has `"status":403`, regardless of any query parameter, and no file is changed
