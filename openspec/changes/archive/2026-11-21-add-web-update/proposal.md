## Why

Updating the firmware today means pulling the SD card (or connecting a CM4 to `rpiboot`), copying files and putting it back. First-time setup means editing two text files on the card in a computer. For a device that sits behind a TV both are the main reasons it is set up once and never updated. The device also cannot be found on the network other than by its DHCP lease.

## What Changes

- **HTTP API on port 80** (no web pages): requests as URL-encoded or multipart form data, responses as JSON. Meant for a companion app (separate project) and for `curl`.
  - `GET /api/status`: `device_id`, board, firmware version, build, mode (`normal` or `setup`), network interface, IP address, MQTT state, uptime.
  - `POST /api/update`: upload of the release zip from GitHub (`raspir-blaster-<version>-sdcard.zip`, or `-cm4-emmc.zip` on a CM4 with eMMC). The firmware unpacks it (deflate decoder in `src/core`), checks the manifest in the zip (right build, SHA-256 of the kernel), and installs **only the kernel image for its own board** (written and read back, old kernel kept as `.bak`, then renamed and rebooted).
  - **Version warning:** if the uploaded version is not newer than the installed one (or either is a `dev` build), the response is `confirm_required` with a token; `POST /api/update/confirm` installs it. Wrong build or wrong checksum is refused.
  - `POST /api/restart`.
- **Normal mode** (configuration present): update and restart need the new key `ota_password`; without it they are disabled. The configuration cannot be changed over the network in normal mode.
- **Setup mode** (replaces the degraded state on boards with a Wi-Fi chip): if `raspir-blaster.conf` is missing or invalid, or a board without Ethernet has no `wpa_supplicant.conf`, the device opens the Wi-Fi access point `raspir-blaster-<last 4 hex digits of its MAC>` with the address 192.168.4.1 and hands out addresses to clients itself (DHCP). The API then needs no password and additionally offers:
  - `GET /api/config` (current values, without secrets) and `POST /api/config` (all keys of `raspir-blaster.conf` plus Wi-Fi SSID, password and country; validated, writes both files),
  - restart and update as in normal mode.
  - The access point is **open** (Circle supports no WPA2 access point) and closes after 30 minutes without an API request; the device then stays in the degraded state until it is powered off and on.
- **Release manifest:** `make sdcard` writes `raspir-blaster-manifest.txt` (version, build, SHA-256 of each kernel) into `build/sdcard/` and `build/cm4-emmc/`, so every release zip contains it.
- **Announcement on the local network (mDNS / Bonjour):** in both modes the device answers `<hostname>.local` and publishes an `_http._tcp` service "raspir-blaster (<device_id>)" with version, board and mode.
- **Host name:** `device_id` is used directly for DHCP and mDNS. Its allowed characters are restricted to `[a-z0-9-]` (default changes from `pi_zero_ir` to `pi-zero-ir`), since `_` is not allowed in host names and `device_id` is also the MQTT client ID and topic component.
- **Documentation** of the API with `curl` examples in `docs/api.md`.

## Capabilities

### New Capabilities
- `web-update`: the HTTP API (status, update with validation and version warning, restart), the password rule, and setup mode (access point, DHCP, configuration endpoints, timeout).

### Modified Capabilities
- `connectivity`: host name derived from `device_id`; new requirement for the mDNS announcement.
- `device-config`: new key `ota_password` (masked like `mqtt_password`); missing configuration leads to setup mode instead of the degraded state on boards with a Wi-Fi chip.
- `firmware-build`: the SD card and eMMC directories contain the manifest.

## Non-goals

- HTML pages, the companion app itself, and an HA `update` entity.
- Changing the configuration over the network in normal mode.
- Wi-Fi network scan in setup mode (the SSID is entered by the user).
- A WPA2-protected access point (not supported by Circle), HTTPS, user accounts, rate limiting.
- Automatic rollback to the old kernel if the new one does not start (Raspberry Pi "tryboot"); planned as a follow-up. Until then `.bak` allows a manual restore.
- Updating boot firmware, `config.txt`, device trees or Wi-Fi firmware from the zip.
- Setup mode on the CM4 without wireless (no Wi-Fi chip): it keeps the degraded state.

## Impact

- New in `src/core` (host-tested): `inflate`, `zipreader`, `sha256`, `manifest` (parse, version compare), `hostname`, `formdata` (URL-encoded fields), `jsonwriter`, `dhcpserver` (packet handling), `configwriter` (writes `raspir-blaster.conf` and `wpa_supplicant.conf`).
- New in `src/fw`: `CApiServer` (Circle `CHTTPDaemon`), `CFirmwareUpdater`, `CDHCPServer` (UDP), setup-mode start in `CKernel`; `src/fw/Makefile`.
- `src/core/config` (`ota_password`); `scripts/sdcard.sh` (manifest); the release workflow is unchanged.
- RAM: up to 8 MB for an upload, plus the extracted kernel (~1 MB).
- README (setup mode, updates, `.local` name, `ota_password`), `docs/api.md`, CHANGELOG, CLAUDE.md; hardware tests added to `verify-ir-blaster-hardware`.
