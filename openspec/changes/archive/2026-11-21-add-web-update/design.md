## Context

The firmware has no HTTP server. The boot medium is FAT (FatFs, writable: the Ethernet change already creates and deletes a marker file). Release zips (`raspir-blaster-<version>-sdcard.zip` 5.8 MB, `-cm4-emmc.zip` 3.2 MB for 2026.11.2) are deflate-compressed and contain all kernels of their build plus boot files. The SD card zip and the eMMC zip both contain a `kernel8-rpi4.img`, but they are different builds (`USE_EMBEDDED_MMC_CM`). Today a missing or invalid configuration leads to the degraded state (slow blink, no network).

Relevant Circle facts (Step51.1.1, verified in `toolchain/circle`):
- `CHTTPDaemon (pNet, pSocket, nMaxContentSize, nPort, nMaxMultipartSize, nTimeoutSeconds)` is a `CTask`; the listener creates one worker per connection (up to 10) through `CreateWorker()`. `GetContent (pPath, pParams, pFormData, pBuffer, pLength, ppContentType)` produces the response; `GetMultipartFormPart()` returns the parts of a multipart POST. Only `application/x-www-form-urlencoded` bodies of at most `HTTP_MAX_FORM_DATA` (2048) bytes are passed as `pFormData`; JSON request bodies are not supported. **Request headers are not accessible**, so HTTP Basic authentication is impossible; the password is a form field. `sample/38-bootloader` uploads kernel images this way.
- `CmDNSDaemon` (singleton, `Get()`) answers `<hostname>.local` with the net subsystem's host name and resolves conflicts with a `-2`, `-3`, ... suffix (`GetHostname()` returns the name in use). `CmDNSPublisher (pNet)` is a `CTask`; `PublishService (name, type, port, ppText)` announces a DNS-SD service with up to 10 TXT records.
- `CBcm4343Device::CreateOpenNet (pSSID, nChannel, bHidden)` / `DestroyOpenNet()` create an **open** access point; there is no WPA2 access point (`addon/wlan/sample/hello_ap`, which uses a static IP via `CNetSubSystem (IPAddress, NetMask, …, NetDeviceTypeWLAN)`). Circle has **no DHCP server**.
- Circle has no zip, deflate or SHA-256 implementation.

## Goals / Non-Goals

**Goals:**
- One API for a companion app: status, update, restart in normal mode; additionally configuration in setup mode.
- Update with the unmodified release zip, safe against wrong, damaged or older files.
- First-time setup without a computer: phone joins the device's access point, the app sends the configuration.
- All parsing and packet logic host-testable in `src/core`.

**Non-Goals:**
- See proposal (HTML, the app, config changes in normal mode, Wi-Fi scan, WPA2 AP, HTTPS, tryboot rollback, boot-file updates, setup mode without a Wi-Fi chip).

## Decisions

### D1 — Hardware-independent parts in `src/core`
- `inflate.{h,cpp}`: raw DEFLATE decoder (RFC 1951: stored, fixed and dynamic Huffman blocks) into a caller-provided buffer with a size limit; returns an error instead of overrunning. Written for this project (≈ 350 lines, style of the well-known "tinf" design), no allocation.
- `zipreader.{h,cpp}`: finds the end-of-central-directory record, looks up an entry by name, follows the local header, extracts "stored" or "deflate" entries via `inflate`, and checks the CRC-32. No ZIP64 (zips are < 10 MB), no encryption.
- `sha256.{h,cpp}`: FIPS 180-4, incremental (`Update`/`Final`) for the read-back.
- `manifest.{h,cpp}`: parses `raspir-blaster-manifest.txt` (`version`, `build`, `sha256.<image>`); `VersionCompare()` for `yyyy.mm.rm` (numeric per field; "not comparable" for anything else).
- `device_id`'s allowed characters (`config.cpp`) are restricted to `[a-z0-9-]` (no `_`), so it is itself a valid host name and is used directly — no separate hostname-derivation module.
- `formdata.{h,cpp}`: looks up a field in URL-encoded data and decodes `%xx` and `+`, with a size limit.
- `jsonwriter.{h,cpp}`: builds JSON objects into a buffer (`String`, `Number`, `Bool`, escaping of `"`, `\` and control characters), on top of `CTextWriter`.
- `dhcpserver.{h,cpp}`: `DHCPHandle (request, response)` answers DISCOVER with OFFER and REQUEST with ACK (or NAK) for a fixed pool (192.168.4.10–50, lease 3600 s, server and subnet options, no router/DNS); lease table of 41 entries by MAC. Pure packet logic, no sockets.
- `configwriter.{h,cpp}`: validates a set of form fields with the rules of `config` (re-using `ConfigParse` on the generated text, so the rules exist only once) and produces the text of `raspir-blaster.conf` and `wpa_supplicant.conf` (quoting of SSID and password; refuses `"` and control characters in them).
Tests use small zips generated once with Python's `zipfile` (stored and deflated entries, a corrupted copy, a truncated file) in `tests/data/`, SHA-256 test vectors, recorded DHCP DISCOVER/REQUEST packets from macOS and iOS, and a round trip over a real release zip if `RASPIR_TEST_RELEASE_ZIP` is set.

### D2 — API server: `CApiServer : CHTTPDaemon` on port 80
Endpoints as in the spec; `GetContent()` dispatches on the path and returns JSON built with `jsonwriter`. `nMaxMultipartSize` 8 MB + 64 KB (form overhead), `nMaxContentSize` 4 KB, receive timeout 60 s. The server knows the mode (`normal`/`setup`) and refuses mode-specific endpoints. Password: constant-time compare with `ota_password`; after a wrong password the worker sleeps 1 s (`CScheduler::MsSleep`, other tasks keep running). Every request resets the setup-mode inactivity timer.

**`GetContent()` always returns `HTTPOK` (200).** Circle's `CHTTPDaemon::Worker()` (`lib/net/httpdaemon.cpp`) unconditionally replaces the response body with its own generated HTML page and resets `Content-Type` to `text/html` whenever `GetContent()` returns anything other than `HTTPOK` — this happens in a private, non-virtual method with no override point (`CreateWorker`, `GetContent` and `WriteAccessLog` are the only three virtuals). So a real non-200 status line can never carry a JSON body through this daemon. Given the choice of (a) real status codes with an HTML error body, (b) forking Circle's `httpdaemon.cpp` into our tree to add a hook, or (c) always 200 with the logical status in the JSON body, (b) was rejected — it is vendoring Circle's GPLv3 source into an Apache-2.0 tree, both against `CLAUDE.md` ("never vendor `toolchain/` contents elsewhere") and a license mismatch for the copied file — and (a) was rejected as not JSON-only. We use (c): the wire HTTP status is always 200; the logical status for an error lives in the JSON body's `"status"` field alongside `"result":"error"` and `"message"`.

**Exception: errors `CHTTPDaemon::ParseRequest()` detects before calling `GetContent()`.** A request larger than `nMaxMultipartSize` (`POST /api/update`'s 8 MB file), a malformed request line, or an over-long URI are rejected by `ParseRequest()` itself (`Worker()`'s `if (Status == HTTPOK) { Status = GetContent (...); }` never runs). `GetContent()` is never reached, so (c) above cannot apply: these get Circle's real status (413, 400, 414) with its own HTML body, same as the "real status + HTML body" option rejected everywhere else — it is simply unavoidable here. This only affects the oversized-upload case in this API (`POST /api/update`'s "file too large"); every status 400/403/404/409 this API itself decides on goes through `GetContent()` and gets the 200+JSON treatment.

### D3 — `CFirmwareUpdater` (`src/fw`)
- Build identity at compile time: image name from `RASPPI` (1 → `kernel.img`, 3 → `kernel8.img`, 4 → `kernel8-rpi4.img`), build from `USE_EMBEDDED_MMC_CM` (`cm4-emmc`, otherwise `sdcard`).
- `Validate (zip)`: manifest → build matches → extract own image (heap buffer of the uncompressed size, max 4 MB) → SHA-256 matches → version comparison. The extracted image becomes the pending update (one slot, random 128-bit token from the hardware RNG, 10-minute expiry; a new upload replaces it). A `busy` flag refuses parallel uploads (cooperative scheduler, no lock needed).
- `Install()`: write `<image>.new` in 64 KB chunks (yielding between chunks), `f_sync`, close; reopen, read back and hash; `f_unlink (<image>.bak)`; `f_rename (<image>, <image>.bak)`; `f_rename (<image>.new, <image>)`. If the last rename fails, rename `.bak` back. On success the kernel sets a reboot time (2 s, so the response is delivered) and the main loop calls `CBcmWatchdog::Restart()`. `POST /api/restart` uses the same reboot path.
*Alternative:* write directly over `<image>`. Rejected: a failure in the middle would leave no bootable kernel.

### D4 — Manifest from `scripts/sdcard.sh`
`assemble` writes `raspir-blaster-manifest.txt` after copying the images: the version from `build/generated/firmware_version.h`, the build name, and `shasum -a 256` of each kernel in the directory. The release workflow packages `build/sdcard/` and `build/cm4-emmc/` as before, so release zips get the manifest without workflow changes.

### D5 — Setup mode in `CKernel`
`Initialize()` decides the mode after `LoadConfig()` and `NetSelect()`: if the configuration is unusable, or the selection is `NetChoiceNone`, and the board has a Wi-Fi chip (`m_WLAN.Initialize()` succeeds), the kernel
1. creates `CNetSubSystem (192.168.4.1, 255.255.255.0, 0, 0, <hostname>, NetDeviceTypeWLAN)`,
2. calls `CreateOpenNet ("raspir-blaster-<xxxx>", 6, false)` (MAC from the WLAN device),
3. starts `CDHCPServer` (a `CTask` with a UDP socket on port 67, broadcast replies, using `dhcpserver`), the API server and mDNS,
4. runs the main loop in setup mode: watchdog, LED in the degraded pattern, USB log, timeout check.
After 30 minutes without an API request it calls `DestroyOpenNet()`, logs it and continues in the degraded loop (no reboot loop). The WPA supplicant is not started in setup mode. If the Wi-Fi chip cannot be initialized, the kernel falls back to the existing degraded state.
*Alternative:* station + AP at the same time. Rejected: not supported by Circle's driver, and without configuration there is nothing to connect to.

### D6 — Configuration endpoints write files atomically
`POST /api/config` → `configwriter` → write `raspir-blaster.conf.new` / `wpa_supplicant.conf.new`, `f_sync`, then rename over the old files (delete old first, FatFs cannot rename over an existing file). Values not given keep their defaults (setup mode starts from no usable configuration). The configuration is not applied live; the app calls `POST /api/restart`.

### D7 — mDNS in both modes
The net subsystem gets the host name from `device_id` directly (DHCP uses it too; in setup mode without a usable configuration, the default `device_id`) — no transformation, since `device_id`'s own character set is already host-name-safe. When the network comes up (normal mode) or the access point is open (setup mode), the kernel creates `CmDNSPublisher` and publishes `raspir-blaster (<device_id>)` / `_http._tcp` / 80 with TXT `version`, `board`, `mode`, `api=/api/`, then logs `CmDNSDaemon::Get()->GetHostname()` + `.local`.

### D8 — `ota_password` in the configuration
Parsed like `mqtt_password` (8–63 chars, else ignored with a warning without the value), masked as `***` in the config dump; without it update and restart answer 403 in normal mode.

## Risks / Trade-offs

- [Open access point in setup mode: anyone in Wi-Fi range can configure the device or install firmware while it is unconfigured] → only while configuration is missing, closes after 30 minutes without requests, logged; documented in the README. The maintainer chose this over a setup code.
- [Plain HTTP in normal mode: password and zip travel unencrypted in the LAN] → documented; acceptable for a home LAN; HTTPS and signing are non-goals.
- [Phones may drop a Wi-Fi without internet access or route API requests via mobile data] → no router/DNS in the DHCP offer (most phones then keep the Wi-Fi for local traffic); the app must bind to the Wi-Fi network (documented in `docs/api.md`).
- [Own DHCP server interoperability] → recorded packets from macOS/iOS/Android in host tests; hardware test with an iPhone and an Android phone.
- [Heavy work in the HTTP worker task (inflate ≈ 1 MB, SHA-256, SD writes) delays the main task] → chunked writes yield; IR receive runs in the ISR; measured on hardware.
- [Memory: an 8 MB multipart buffer per active upload connection] → one update at a time; 512 MB on the smallest board.
- [Power loss during the two renames leaves no `<image>`] → window of a few milliseconds; `.bak` allows a manual restore.
- [`.local` names not resolvable in some setups (Docker, Android)] → the IP address is logged and in the DHCP lease list; the setup address is fixed (192.168.4.1).
- [`device_id`'s allowed characters change (`_` no longer allowed): an existing config with an underscore in `device_id` now falls back to the new default `pi-zero-ir`, changing the MQTT client ID, topics and Home Assistant identifiers, not just the host name] → noted prominently in CHANGELOG and README; the boot log's "invalid value ... for 'device_id' - using default" warning already names the rejected value.

## Migration Plan

Existing cards keep working: without `ota_password` the API is read-only (status). A card without configuration now opens the setup access point instead of only blinking. The first update to a firmware with this feature is still done by copying the zip to the card. Rollback: copy an older release zip to the card, or rename `.bak`.
