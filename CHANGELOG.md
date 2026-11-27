# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).
This project does not use Semantic Versioning: releases are numbered
`yyyy.mm.rm` (year, month, release counter within the month), as described in
the [README](README.md#releases).

## [Unreleased]

### Added

- Two Home Assistant scripts in `ha/scripts/`, "Learn IR code" and "Forget IR
  code": press a button on your remote, give it a name, and Home Assistant
  creates a button that sends this code. A step-by-step guide with
  screenshots is in `docs/train-ir-remote.md`.

## [2026.11.5] - 2026-11-25

### Added

- Home Assistant now also gets an "IR Event" event entity, which fires once
  per received IR frame, also when the same frame arrives twice in a row
  (the "IR Receiver" sensor ignores such repeats). Use it to trigger
  automations; the sensor stays unchanged.

## [2026.11.4] - 2026-11-24

### Added

- `make DIAG_SDLOG=1` builds diagnostic images that also append the log to
  `raspir-diag.log` on the SD card, for when neither a serial adapter nor
  the USB log is available.

### Fixed

- After startup the device rebooted every ~18 s and never joined the
  network or connected to MQTT (seen on a Zero W with 2026.11.3): the mDNS
  announcement was started before the network was up and blocked the main
  loop until the watchdog reset the board. The USB log stayed empty for the
  same reason. mDNS now starts once the network has an IP address.
- README, the configuration example and the specification now state that
  `mqtt_host` cannot be a `.local` (mDNS) name such as
  `homeassistant.local`; use the broker's IP address.
- After a release, the release workflow copied all of that release's
  changelog entries back under `[Unreleased]` on `main` (2026.11.3), so the
  next release would have listed them again. It now keeps only the entries
  merged to `main` while the release was building.

## [2026.11.3] - 2026-11-22

### Added

- HTTP API on port 80 (`GET /api/status`, `GET`/`POST /api/config`;
  `POST /api/update`, `/api/update/confirm`, `/api/update/cancel` and
  `/api/restart` with `ota_password`): upload a release zip to update the
  firmware over the network instead of swapping SD card files by hand, or
  read/change `raspir-blaster.conf` and the Wi-Fi network remotely. See
  [`docs/api.md`](docs/api.md).
- Setup mode: boards with a Wi-Fi chip open an access point
  (`raspir-blaster-<xxxx>`) and serve the API without a password when
  `raspir-blaster.conf` is missing or invalid, or Wi-Fi is needed and
  `wpa_supplicant.conf` is missing. The access point closes after
  30 minutes without an API request.
- mDNS (`<hostname>.local`, `_http._tcp` service with version/board/mode
  TXT records) in both normal and setup mode.
- `ota_password` configuration key (8–63 chars): required for the update
  and restart endpoints in normal mode; without it those endpoints answer
  403.

### Changed

- `device_id` is now used as-is for the DHCP host name and the mDNS
  `.local` name (previously it was used unchanged, which showed any `_` it
  contained — not a valid host name character). To make this possible,
  `device_id`'s allowed characters change from `[a-z0-9_]` to `[a-z0-9-]`
  (default `pi_zero_ir` → `pi-zero-ir`); an existing `device_id` containing
  `_` is now rejected and falls back to the new default, which also
  changes the MQTT client ID, topics and Home Assistant identifiers, not
  just the host name. Check the boot log for an "invalid value ... for
  'device_id'" warning after updating.
- A missing or invalid `raspir-blaster.conf` (or, on a Wi-Fi-only board, a
  missing `wpa_supplicant.conf`) enters setup mode instead of the degraded
  state, on boards with a Wi-Fi chip. A CM4 without wireless still enters
  the degraded state.
- The release workflow now brings `CHANGELOG.md` back to `main` itself after
  publishing a release, instead of relying on a maintainer to open a pull
  request for it. This replaces a step that was twice forgotten and only
  noticed when the following release failed.

## [2026.11.2] - 2026-11-21

### Added

- Ethernet on the Raspberry Pi 3 B/B+, Pi 4 B and Compute Module 4. At boot
  the firmware uses Ethernet if a cable has link within 5 s, otherwise Wi-Fi
  (Pi 4 B and CM4 reboot once to switch to Wi-Fi).
- Support for the Compute Module 4 without wireless (Ethernet only).
- Log over USB on the Zero W, Zero 2 W, Pi 3 A+ and Pi 4 B: with one USB
  cable to a computer the log appears as a USB serial port (e.g.
  `/dev/cu.usbmodem*` on a Mac). Opening the port first shows the last
  16 KB of log, including the boot log. The UART log is unchanged.

### Changed

- `wpa_supplicant.conf` is only required on boards without Ethernet (Zero W,
  Zero 2 W, Pi 3 A+). Without it, boards with Ethernet use Ethernet instead of
  stopping with a configuration error.

## [2026.11.1] - 2026-11-20

### Added

- 3D-printable enclosure for the Raspberry Pi Zero W in `case/`: SolidCraft
  source (`raspir-blaster.sc`), print files (`Case.stl`, `Top Lid.stl`,
  `raspir-blaster-case.3mf`) and the Pi Zero W mechanical drawing used as
  reference.
- This changelog. Each release turns the `[Unreleased]` entries into a dated
  section on its release branch, and the release notes show that section.

### Changed

- The release workflow stops before creating the release branch if
  `[Unreleased]` is empty, or if `CHANGELOG.md` on `main` does not contain the
  previous release yet.

## [2026.11.0] - 2026-11-20

First release. The firmware builds for all supported boards but has not been
verified on hardware yet.

### Added

- Bare-metal firmware built on [Circle](https://github.com/rsta2/circle), with
  no Linux, that bridges infrared and Home Assistant over MQTT.
- IR receive: frames from any remote are captured on GPIO23 (header pin 16) as
  raw mark/space timings and published to `<topic_base>/received`. A
  configurable silence (`ir_frame_gap_ms`) ends a frame, so long
  air-conditioner frames arrive in one piece.
- IR transmit: raw timings published to `<topic_base>/send` are sent on GPIO18
  (header pin 12) with a PWM carrier of 20–60 kHz (default 38 kHz).
- Strict validation of IR messages (at most 4096 bytes, 512 timings of
  1–65535 µs each, 2 s in total). Invalid messages are rejected and logged,
  and retained messages on the send topic are ignored.
- Home Assistant MQTT discovery: the device appears automatically with an
  "IR Receiver" sensor that reports the board model and firmware version.
- Availability through `<topic_base>/status` (`online`/`offline`, retained,
  with last will), and reconnection to the broker with backoff (5 s up to
  60 s).
- Configuration file `raspir-blaster.conf` on the boot partition for the
  broker, credentials, `device_id`, `topic_base` and `ir_frame_gap_ms`. A
  missing or invalid configuration puts the device into a degraded state
  (slow ACT LED blink) without a reboot loop.
- Hardware watchdog that reboots the device 10 s after a hang or driver
  failure.
- Builds for Raspberry Pi Zero W, Zero 2 W, 3 B/B+/A+, 4 B, Compute Module 4
  Lite and Compute Module 4 with eMMC.
- Release downloads: `raspir-blaster-<version>-sdcard.zip` (universal SD card
  for all boards except CM4 with eMMC) and
  `raspir-blaster-<version>-cm4-emmc.zip`, with SHA-256 checksums in
  `SHA256SUMS.txt`.
- macOS build with one `make`: the toolchain and Circle are downloaded and
  pinned automatically, boot and Wi-Fi firmware are verified by SHA-256.
- GitHub Actions: CI builds and tests every pushed commit, and a manual
  release workflow publishes versioned releases.

[unreleased]: https://github.com/home-assistant-integrations/raspir-blaster/compare/2026.11.5...HEAD
[2026.11.5]: https://github.com/home-assistant-integrations/raspir-blaster/compare/2026.11.4...2026.11.5
[2026.11.4]: https://github.com/home-assistant-integrations/raspir-blaster/compare/2026.11.3...2026.11.4
[2026.11.3]: https://github.com/home-assistant-integrations/raspir-blaster/compare/2026.11.2...2026.11.3
[2026.11.2]: https://github.com/home-assistant-integrations/raspir-blaster/compare/2026.11.1...2026.11.2
[2026.11.1]: https://github.com/home-assistant-integrations/raspir-blaster/compare/2026.11.0...2026.11.1
[2026.11.0]: https://github.com/home-assistant-integrations/raspir-blaster/releases/tag/2026.11.0
