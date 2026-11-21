## 1. Core: checksums, manifest, names, form and JSON

- [x] 1.1 `src/core/sha256.{h,cpp}` (incremental) + tests with the FIPS 180-4 vectors ("", "abc", 1 million "a")
- [x] 1.2 `src/core/manifest.{h,cpp}`: parse `version`, `build`, `sha256.<image>`; `VersionCompare()` + tests (newer, same, older, `2026.11.10` vs `2026.11.9`, `dev`, malformed lines)
- [x] 1.3 `src/core/hostname.{h,cpp}` (`_` → `-`) + tests
- [x] 1.4 `src/core/formdata.{h,cpp}` (field lookup, `%xx` and `+` decoding, limits) + tests
- [x] 1.5 `src/core/jsonwriter.{h,cpp}` (objects, strings with escaping, numbers, booleans, buffer limit) + tests

## 2. Core: zip and deflate

- [x] 2.1 `src/core/inflate.{h,cpp}`: stored and fixed-Huffman blocks, output limit
- [x] 2.2 Dynamic-Huffman blocks; error on invalid code lengths/distances
- [x] 2.3 `src/core/zipreader.{h,cpp}`: end-of-central-directory, entry lookup, local header, extract with CRC-32 check
- [x] 2.4 Test data in `tests/data/` (generated with Python `zipfile`: stored + deflated entries, a corrupted copy, a truncated file) and a generator script; tests for inflate and zip reader, including a real release zip when `RASPIR_TEST_RELEASE_ZIP` is set

## 3. Core: setup mode

- [x] 3.1 `src/core/dhcpserver.{h,cpp}`: DISCOVER → OFFER, REQUEST → ACK/NAK, pool 192.168.4.10–50, lease table, options (subnet, server id, lease time)
- [x] 3.2 DHCP tests with recorded DISCOVER/REQUEST packets (macOS, iOS, Android), pool exhaustion, renewal by the same MAC
- [x] 3.3 `src/core/configwriter.{h,cpp}`: validate fields via `ConfigParse`, generate `raspir-blaster.conf` and `wpa_supplicant.conf` (WPA2-PSK / open), refuse `"` and control characters + tests

## 4. Configuration and build

- [x] 4.1 `ota_password` in `src/core/config` (8–63 chars, warning without the value, masked in the dump) + tests
- [x] 4.2 `scripts/sdcard.sh`: write `raspir-blaster-manifest.txt` for both directories; check with `shasum -a 256` and the scenario in the firmware-build spec

## 5. Firmware: API and update

- [x] 5.1 Host name from `HostnameFromDeviceID()` for `CNetSubSystem`; mDNS publisher with TXT records, log of the `.local` name (D7)
- [x] 5.2 `CApiServer`: dispatch, JSON responses with status codes, 404 for unknown paths, `GET /api/status`
- [x] 5.3 Password check (constant-time, 1 s delay, log without value) and mode checks (403)
- [x] 5.4 `CFirmwareUpdater::Validate()`: manifest, build, extraction, checksum, version comparison, pending slot with token and expiry (D3)
- [x] 5.5 `CFirmwareUpdater::Install()`: `.new` write in chunks, read-back hash, `.bak` and renames with restore on failure
- [x] 5.6 `POST /api/update`, `POST /api/update/confirm`, `POST /api/restart`; reboot from the main loop

## 6. Firmware: setup mode

- [x] 6.1 Mode decision in `CKernel::Initialize()`; access point with static address and SSID from the MAC; fallback to degraded without Wi-Fi chip (D5)
- [x] 6.2 `CDHCPServer` task (UDP port 67, broadcast replies) using `dhcpserver`
- [x] 6.3 `GET /api/config` and `POST /api/config` with atomic file writes (D6)
- [x] 6.4 Setup main loop: inactivity timeout of 30 minutes → `DestroyOpenNet()` → degraded loop; API, mDNS and USB log in setup mode
- [x] 6.5 `src/fw/Makefile`; `make` for all targets without warnings; image sizes noted
- [ ] 6.6 QEMU check (watchdog-free scratch build): normal-mode boot unchanged; setup-mode path on `raspi3b` up to "Cannot initialize Wi-Fi" → degraded (QEMU has no Wi-Fi chip)

## 7. Documentation

- [x] 7.1 `docs/api.md`: all endpoints with fields, responses, status codes and `curl` examples; notes for app developers (bind to the setup Wi-Fi, no internet there)
- [x] 7.2 README: first setup via the access point, updating via the API, manual restore from `.bak`, `.local` name and service browsing, `ota_password` in the configuration table, the open-AP warning
- [x] 7.3 `CHANGELOG.md` (Added: API, update, setup mode, mDNS; Changed: DHCP host name `_` → `-`, setup mode instead of degraded on missing configuration); `CLAUDE.md` (API server, updater, setup mode, Circle HTTP/mDNS/AP facts)
- [x] 7.4 Hardware tests in `verify-ir-blaster-hardware`: setup with an iPhone and an Android phone (AP visible, DHCP, `POST /api/config`, restart into normal mode), setup timeout, update via the API (newer, same version, wrong build zip, damaged zip, wrong password), `.bak` restore, `.local` name and `dns-sd` browse
