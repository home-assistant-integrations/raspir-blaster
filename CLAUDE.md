# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

# `raspir-blaster`

Bare-metal IR blaster and receiver for a Raspberry Pi Zero W, built on the [Circle](https://github.com/rsta2/circle) C++ framework (no Linux). It bridges Home Assistant and infrared over MQTT: payloads from HA are transmitted as IR, and IR from physical remotes is published back to HA.

Technical details are described in the [technical design document](docs/dev/raspir-blaster-home-assistant.md). The authoritative requirements are in `openspec/specs/`; the design decisions behind them are in the `design.md` files of the archived changes in `openspec/changes/archive/` (`add-ir-blaster-firmware`: firmware architecture; `add-multi-board-builds`: build targets and boot media; `add-ci-release-workflows`: CI, releases and versioning; `add-release-changelog`: changelog in releases; `add-ethernet-support`: Ethernet and network selection).

## Commands

```sh
make                    # bootstrap toolchain/ on first run, then build all targets into build/images/<target>/
make TARGETS=rpi3       # build only some targets (rpi0w rpi3 rpi4 rpi4-emmc)
make test               # host unit tests for src/core (clang++, ASan/UBSan)
make sdcard             # build/sdcard/ (universal card) + build/cm4-emmc/; needs all four images
make firmware-checksums # refresh scripts/firmware-files.txt after bumping a pinned firmware commit
make clean / distclean  # distclean also removes toolchain/
make WATCHDOG_TEST=1    # test images whose main loop hangs after 30 s (watchdog must reboot it)
make VERSION=2026.11.0  # firmware version; default: VERSION file (release branches only), else "dev"
scripts/package-release.sh <version>   # release zips + SHA256SUMS.txt after `make VERSION=<version> sdcard`
```

`make test` also runs the shell tests in `tests/scripts/` (version calculation, changelog).

## CI and releases (`.github/`)

- `workflows/ci.yml`: every push except `rel-*` branches, on `macos-15` (arm64). `actions/build-and-test` (composite, shared with the release) does `make` (fails on compiler warnings), `make test`, `make sdcard`; it caches `toolchain/` except the extracted toolchains, keyed on `versions.env`, `bootstrap.sh` and `firmware-files.txt`.
- `workflows/release.yml`: manual from `main` only; `scripts/next-version.sh` computes `yyyy.mm.rm` from the `rel-*` branches/tags. `scripts/changelog.sh release` moves `[Unreleased]` in `CHANGELOG.md` into a dated section (fails if it is empty or the previous release's changelog was not copied back to `main`). It creates `rel-<version>` with one commit of `VERSION` + `CHANGELOG.md`, builds there, publishes a GitHub release `<version>` (notes: header + `changelog.sh section` + generated notes), and deletes the branch/tag/release on failure. The workflow never pushes to `main`; the changelog comes back via a PR (`git checkout rel-<version> -- CHANGELOG.md`).
- Add a `CHANGELOG.md` entry under `## [Unreleased]` for every user-visible change (Keep a Changelog 1.1.0). `scripts/changelog.sh` uses POSIX `awk` only: it runs with BSD awk locally and mawk on the Ubuntu release runner.
- Never commit a `VERSION` file on `main`: it would set the version of every build. The `VERSION` environment variable is ignored on purpose.
- Pass `${{ }}` values to `run:` only via `env:`.
- The firmware version reaches the code through `build/generated/firmware_version.h` (rewritten only on change; the top-level Makefile then deletes dependent objects because macOS's make 3.81 has one-second timestamp resolution). `src/core/version.h` falls back to `"dev"` via `__has_include`.

There is no single-test filter: `make test` runs every `TEST (...)` in `tests/*.cpp` (fast, < 1 s). New test files are picked up by wildcard.

## Toolchain & build

- The corresponding toolchain as well as the Circle sources are downloaded during the first build into the git-ignored `toolchain/` directory, by `scripts/bootstrap.sh`. Never commit it or vendor its contents elsewhere. Versions (toolchain release + SHA-256, Circle tag) are pinned in `scripts/versions.env`; the bootstrap steps are guarded by stamp files that encode these versions.
- Build host: macOS on Apple Silicon only (ARM publishes no Intel macOS build of the pinned toolchains).
- **Build targets** (`openspec/specs/board-support`):

  | Target | Boards | Circle config | Image |
  | --- | --- | --- | --- |
  | `rpi0w` | Zero W (BCM2835, ARMv6) | `RASPPI=1 AARCH=32`, `arm-none-eabi-` | `kernel.img` |
  | `rpi3` | Zero 2 W, 3B/B+/A+ | `RASPPI=3 AARCH=64`, `aarch64-none-elf-` | `kernel8.img` |
  | `rpi4` | 4B, CM4 Lite | `RASPPI=4 AARCH=64` | `kernel8-rpi4.img` |
  | `rpi4-emmc` | CM4 with eMMC | as `rpi4` + `-DUSE_EMBEDDED_MMC_CM` | `kernel8-rpi4.img` |

  Pi 5/CM5 are out of scope (RP1 I/O chip). Never assume a single SoC: code that touches clocks or peripherals must work on BCM2835, BCM2837 and BCM2711.
- Circle builds its libraries inside its source tree, for one configuration. `toolchain/circle/` is a pristine checkout (with the hostap submodule); the bootstrap makes one APFS clone per target, `toolchain/circle-<target>/`, writes its `Config.mk` and builds it there. Do not build inside `toolchain/circle/`.
- The firmware is built out of tree: the top-level Makefile runs `src/fw/Makefile` in `build/obj/<target>/` with `SRCROOT`/`CIRCLEHOME`. Nothing is written to `src/fw/`.
- Boot files (`raspberrypi/firmware`) and Wi-Fi firmware (`RPi-Distro/firmware-nonfree`) are downloaded by `scripts/sdcard.sh` with `curl` at commits pinned in `versions.env`, listed with SHA-256 in `scripts/firmware-files.txt` (shared helpers in `scripts/firmware-lib.sh`). Circle's own download Makefiles need `wget`, which macOS lacks. The SD card `config.txt` files are tracked in `config/` (board filters `[pi0w]`, `[pi02]`, `[pi3]`, `[pi3+]`, `[pi4]`, `[cm4]`).

## Code layout and architecture

- `src/core/` — hardware-independent logic, compiled both by the host tests and into the firmware: config parser, JSON tokenizer, IR message codec, frame assembler, carrier calculator, backoff, discovery builder, TX queue. It may only include `<stdint.h>`/`<stddef.h>` (no libc, no libc++, no Circle headers); the host build enforces this with `-nostdinc++`, and `coreheaders.cpp` includes every header in both builds. Put new testable logic here.
- `src/fw/` — Circle-dependent code: `CKernel` (devices, main loop), `CIRReceiver` (GPIO23 ISR), `CIRTransmitter` (PWM on GPIO18), `CIRMqttClient`. The fw `Makefile` compiles all `src/core/*.cpp` via `vpath`; add new fw files to `FW_OBJS`. SoC differences are handled with `#if RASPPI ...` in `src/fw` only; `src/core` gets them as parameters (e.g. the oscillator frequency for `IRCarrierCompute`).
- `tests/` — tiny self-made test framework (`test.h`: `TEST`, `CHECK`, `CHECK_EQ`, `CHECK_STR`).

Runtime model (single core, cooperative `CScheduler`):
- **Main task** (`CKernel::Run`): feeds the 10 s watchdog, drains completed RX frames and publishes them, drains the TX queue, updates the ACT LED, then `MsSleep`s to yield. It must never block for long.
- **MQTT task** (`CIRMqttClient`, a `CTask`): runs the connect/backoff state machine in `OnLoop()`, because `CMQTTClient::Connect()` blocks on DNS + TCP. `OnMessage()` only parses and pushes to the TX queue.
- **ISR** (`CIRReceiver::InterruptHandler`): only stores edge timestamps in `CIRFrameAssembler`. The assembler does gap segmentation itself, with two capture buffers, so frames survive the main loop being busy (up to 2 s per transmission). Main-loop calls into the assembler are wrapped in `EnterCritical`/`LeaveCritical`.
- Raw timings everywhere, by design: no protocol decoding. Even index = mark, odd = space. Messages are validated strictly (rejected, never clamped); limits: 4096 B payload, 512 timings, each 1–65535 µs, ≤ 2 s total, `freq` 20–60 kHz.
- States: config missing/invalid → *degraded* (no MQTT, slow ACT blink, watchdog fed, no reboot loop); driver failure → *fatal* (fast blink, watchdog reboots after 10 s).
- Network (`openspec/specs/connectivity`, "Network interface selection"): one interface per boot, chosen in `CKernel::Initialize` via `NetSelect()` (`src/core/netselect.h`). Boards with Ethernet (`BoardHasEthernet()`: 3B, 3B+, 4B, CM4) use Ethernet if `wpa_supplicant.conf` is missing, otherwise Ethernet only with link within 5 s. Pi 3 B/B+ probe the USB chip before the net subsystem exists; Pi 4/CM4 start the net subsystem on Ethernet and, without link, write `SD:/raspir-blaster.wifi` and reboot (`CBcmWatchdog::Restart()`); the next boot deletes the marker first and uses Wi-Fi. The Wi-Fi chip is only initialized for Wi-Fi.
- Pins (identical on all boards): IR TX = GPIO18 / header pin 12 (PWM0, ALT5, `PWM_CHANNEL1`); IR RX = GPIO23 / header pin 16. Serial log on UART0 (GPIO14/15, 115200). The boot log names the board (`CMachineInfo::GetMachineName()`), which is also the HA discovery `model`.

## Circle API facts (verified against Step51.1.1)

The design doc's code snippets predate the implementation and do not match Circle. Trust the headers in `toolchain/circle/include` and the code in `src/fw/`.
- `CMQTTClient::Publish()` always appends a packet identifier, which corrupts QoS 0 PUBLISH packets. **Publish with QoS ≥ 1 only.**
- `CMQTTClient::Connect(host, port, clientId, user, pass, keepAlive, cleanSession, willTopic, willQoS, willRetain, willPayload, willLen)` returns `void`; results arrive via `OnConnect`/`OnDisconnect` (the latter may be called synchronously from `Connect`). Default max packet size is 1024 — the client uses 5120.
- GPIO has no "both edges" interrupt: use `EnableInterrupt(GPIOInterruptOnRisingEdge)` + `EnableInterrupt2(GPIOInterruptOnFallingEdge)`; interrupts need a `CGPIOManager`.
- `CPWMOutput(GPIOClockSourceOscillator, divider, range, bMSMode)` has no runtime frequency setter: the transmitter re-creates it when `freq` changes. It does not mux the pin; set GPIO18 to `GPIOModeAlternateFunction5` yourself. The oscillator is 19.2 MHz on BCM2835/2837 but **54 MHz on BCM2711** (Pi 4/CM4).
- `USE_EMBEDDED_MMC_CM` (CM4 eMMC instead of SD card) is a compile-time switch for all of Circle, including the layout of `CEMMCDevice` in `SDCard/emmc.h`; it is set via `DEFINE +=` in the target's `Config.mk`, which the firmware build inherits through `Rules.mk`.
- 64-bit images are not gzipped (`GZIP_KERNEL` defaults to 0 in Circle's `Rules.mk`).
- `CBcmWatchdog::Start(seconds)` both starts and reloads the watchdog (max 15 s).
- On RASPPI ≤ 3, `USE_SDHOST` is the default: the SD card uses SDHOST, the WLAN uses the EMMC controller.
- `CMQTTClient::Connect` warns if the client ID exceeds 23 characters; `device_id` allows 31.
- `CNetSubSystem` is a singleton with one `TNetDeviceType`; `Initialize()` starts a `CNetTask` that is never stopped, so it cannot be replaced by one of another type. On `RASPPI == 4` the GENET driver (`CBcm54213Device`) is a member of Circle's net device layer and only exists after `CNetSubSystem::Initialize()`. Pi 3 B/B+ Ethernet are USB devices (`CSMSC951xDevice`, `CLAN7800Device`), registered when `CUSBHCIDevice::Initialize()` enumerates them. Link state: `CNetDevice::GetNetDevice (NetDeviceTypeEthernet)`, then `UpdatePHY()` + `IsLinkUp()`.

## Conventions

- Every file that allows comments starts with the header from [docs/dev/development-guidelines.md](docs/dev/development-guidelines.md) (file name, module, author/date, Apache-2.0 license).
- Follow Circle's code style: `C`-prefixed classes, `T`-prefixed types, `m_` members, `p`/`n`/`b` prefixes, tabs; `boolean`/`TRUE`/`FALSE` in `src/fw`, `bool` in `src/core`.
- Use conventional commits.

## Workflow: OpenSpec

The project is spec-driven via OpenSpec (`openspec/config.yaml`). Changes go through `/opsx:propose` → `/opsx:apply` → `/opsx:archive`. The workflow also offers `/opsx:explore`, `/opsx:update` and `/opsx:sync`. Specs live in `openspec/specs/`, and in-flight changes in `openspec/changes/`. Rules from the config:
- Proposals stay under 1000 words and always include a "Non-goals" section.
- Tasks are broken into chunks of at most 15 minutes.
