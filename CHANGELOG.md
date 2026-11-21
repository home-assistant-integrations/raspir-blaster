# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).
This project does not use Semantic Versioning: releases are numbered
`yyyy.mm.rm` (year, month, release counter within the month), as described in
the [README](README.md#releases).

## [Unreleased]

### Added

- 3D-printable enclosure for the Raspberry Pi Zero W in `case/`: SolidCraft
  source (`raspir-blaster.sc`), print files (`Case.stl`, `Top Lid.stl`,
  `raspir-blaster-case.3mf`) and the Pi Zero W mechanical drawing used as
  reference.
- This changelog. Each release turns the `[Unreleased]` entries into a dated
  section on its release branch, and the release notes show that section.
- Ethernet on the Raspberry Pi 3 B/B+, Pi 4 B and Compute Module 4. At boot
  the firmware uses Ethernet if a cable has link within 5 s, otherwise Wi-Fi
  (Pi 4 B and CM4 reboot once to switch to Wi-Fi).
- Support for the Compute Module 4 without wireless (Ethernet only).

### Changed

- `wpa_supplicant.conf` is only required on boards without Ethernet (Zero W,
  Zero 2 W, Pi 3 A+). Without it, boards with Ethernet use Ethernet instead of
  stopping with a configuration error.
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

[unreleased]: https://github.com/home-assistant-integrations/raspir-blaster/compare/2026.11.0...HEAD
[2026.11.0]: https://github.com/home-assistant-integrations/raspir-blaster/releases/tag/2026.11.0
