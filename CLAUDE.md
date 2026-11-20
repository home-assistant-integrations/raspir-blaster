# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

# `raspir-blaster`

Bare-metal IR blaster and receiver for a Raspberry Pi Zero W, built on the [Circle](https://github.com/rsta2/circle) C++ framework (no Linux). It bridges Home Assistant and infrared over MQTT: payloads from HA are transmitted as IR, and IR from physical remotes is published back to HA.

Technical details are described in the [technical design document](docs/dev/raspir-blaster-home-assistant.md).

## Project state

No source code or build system exists yet. The repo contains only the design document, the development guidelines and the OpenSpec scaffolding. It is not yet a git repository.

## Toolchain & build

- **Target SoC: BCM2835** (Raspberry Pi Zero W; single-core ARM1176JZF-S, ARMv6). This fixes the build settings: `RASPPI = 1`, `AARCH = 32` (64-bit is not supported), `PREFIX = arm-none-eabi-` and kernel image `kernel.img`. The Wi-Fi chip is the CYW43438, which uses the `brcmfmac43430-sdio.*` firmware.
- The SoC has a single core. Busy-wait IR transmission and ISRs block everything else, including networking, so keep ISRs minimal.
- The toolchain and the Circle sources are downloaded during the first build and stored in `toolchain/`. This directory is git-ignored. Never commit it or vendor its contents elsewhere.
- The build host is macOS, so the download step must fetch the macOS build of the ARM GNU toolchain.
- How Circle builds work:
  1. A `Config.mk` in the Circle root sets `RASPPI`, `AARCH` and `PREFIX`.
  2. `./makeall` builds the core libraries.
  3. Each addon is built separately with `make` in its own directory: `addon/SDCard`, `addon/fatfs`, `addon/wlan` and `addon/wlan/hostap/wpa_supplicant`.
  4. The app `Makefile` sets `CIRCLEHOME`, lists the `.a` libraries in `LIBS` (link order matters) and includes `$(CIRCLEHOME)/Rules.mk`. The output is a `kernel*.img`.
- SD card (FAT32) contents: the Pi boot firmware (`bootcode.bin`, `start.elf`, `fixup.dat`), the kernel image, the Wi-Fi firmware from Circle's `addon/wlan/firmware` and `wpa_supplicant.conf`. Keep Wi-Fi and MQTT credentials out of the repo.

## Architecture (planned)

- **`CKernel`** owns all devices, and initialization order matters: interrupt → timer → EMMC + FatFs mount `SD:` → WLAN (firmware read from `SD:/firmware/`) → net subsystem → WPA supplicant. WLAN cannot start before the SD card is mounted.
- **Raw timings everywhere, by design.** The firmware does not decode protocols. Signals are arrays of µs durations (even index = mark, odd = space). This covers NEC, Sony, RC5/RC6 and long air-conditioner frames (100–300+ edges) without per-protocol code. Keep it that way unless a change explicitly says otherwise.
- **Pins (fixed by capability):** IR TX is on GPIO18 (header pin 12), because it is the hardware PWM0 pin (Circle `PWM_CHANNEL1`). IR RX (TSOP38238) is on GPIO23 (header pin 16), using an edge interrupt. Don't confuse header pin numbers with GPIO numbers.
- **IR RX:** a GPIO interrupt on both edges stores edge deltas into a buffer of at least 512 entries. A frame ends after 15 ms of silence, and the main loop (not the ISR) publishes it.
- **IR TX:** the carrier comes from hardware PWM at the `freq` given in the payload (36/38/40 kHz) with 33% duty. The RX interrupt is disabled during transmit so the device does not capture its own signal.
- **MQTT payload**, identical in both directions: `{"freq":38000,"raw":[9000,4500,560,...]}`. It is parsed by a small zero-allocation parser (no JSON library). Limits: 4 KB payload, 512 timings, `freq` between 20–60 kHz.
- **Topics:** `home/ir_blaster/send` (command), `home/ir_blaster/received` (state) and `home/ir_blaster/status` (availability: `online`/`offline` via LWT, retained). HA MQTT Discovery configs go under `homeassistant/<component>/pi_zero_ir/...`, published retained after each connect.
- **Main loop:** a reconnect state machine (Wi-Fi up → MQTT connect with a 5 s backoff → subscribe and publish discovery), with an optional `CBcmWatchdog`.

## Open issues in the design doc (resolve before implementing)

1. **The code snippets are illustrative and not checked against Circle's API.** Verify them against the headers in `toolchain/circle/include` and the Circle samples before reusing anything. Known gaps:
   - Circle networking and `CMQTTClient` run as `CTask`s, which need a `CScheduler`. The main loop must yield (scheduler sleep), not busy-wait with `MsDelay`.
   - GPIO interrupts need a `CGPIOManager`.
   - `CMQTTClient::Connect` is asynchronous (results arrive through `OnConnect`/`OnDisconnect`), and its signature differs from the doc's.
   - The default max MQTT packet size is too small for 4 KB payloads.
   - The doc's PWM frequency API (`SetClockRate`) does not match `CPWMOutput`.
   - Markdown rendering stripped escapes from the snippets (unescaped quotes in JSON string literals, `' '` where `'\0'` was meant).

## Conventions

- Every file that allows comments starts with the header from [docs/dev/development-guidelines.md](docs/dev/development-guidelines.md) (file name, module, author/date, Apache-2.0 license).
- Follow Circle's code style, as the design doc does: `C`-prefixed classes, `m_` members, `p`/`n`/`b` prefixes, `boolean`/`TRUE`/`FALSE`.
- Use conventional commits.

## Workflow: OpenSpec

The project is spec-driven via OpenSpec (`openspec/config.yaml`). Changes go through `/opsx:propose` → `/opsx:apply` → `/opsx:archive`. The workflow also offers `/opsx:explore`, `/opsx:update` and `/opsx:sync`. Specs live in `openspec/specs/`, and in-flight changes in `openspec/changes/`. Rules from the config:
- Proposals stay under 1000 words and always include a "Non-goals" section.
- Tasks are broken into chunks of at most 15 minutes.
