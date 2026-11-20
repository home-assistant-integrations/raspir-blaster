# firmware-build Specification

## Purpose
How the firmware is built on the macOS host: toolchain and Circle bootstrap, build targets, version embedding, host unit tests and the SD card / eMMC boot media.
## Requirements
### Requirement: One-command build on macOS
The project SHALL provide one top-level build command, run from the repository root on a macOS host with Apple Silicon (the pinned ARM GNU Toolchain release has no Intel macOS build), that produces the kernel images of all build targets defined in `board-support`: `kernel.img` (`rpi0w`), `kernel8.img` (`rpi3`), `kernel8-rpi4.img` (`rpi4`) and `kernel8-rpi4.img` (`rpi4-emmc`). Each image SHALL be written to `build/images/<target>/`.

#### Scenario: First build on a clean checkout
- **WHEN** the developer runs the top-level build command on a fresh clone without a `toolchain/` directory
- **THEN** the build downloads all dependencies, builds Circle and its addons once per target, and produces all four kernel images, all without manual steps

#### Scenario: Incremental build
- **WHEN** the developer changes a firmware source file and runs the build command again
- **THEN** only the affected firmware objects of each target are rebuilt, and neither the toolchains nor Circle are downloaded or rebuilt again

### Requirement: Toolchain and Circle are downloaded into `toolchain/`
The build SHALL download the ARM GNU toolchains `arm-none-eabi` (for AArch32) and `aarch64-none-elf` (for AArch64), both as `darwin-arm64` builds of the same release, and the Circle sources into the git-ignored `toolchain/` directory. Toolchains and Circle SHALL be pinned to explicit versions recorded in the repository. Every toolchain archive SHALL be checked against a recorded SHA-256 checksum.

#### Scenario: Pinned versions
- **WHEN** the toolchains and Circle are downloaded
- **THEN** the exact toolchain release and Circle tag recorded in the repository are used for both toolchains

#### Scenario: Checksum mismatch
- **WHEN** a downloaded toolchain archive does not match its recorded SHA-256 checksum
- **THEN** the build aborts with an error naming the archive, and nothing is extracted into `toolchain/`

#### Scenario: Nothing downloaded outside `toolchain/`
- **WHEN** the build completes
- **THEN** all downloaded third-party content resides under `toolchain/`, and `git status` shows no untracked third-party files

### Requirement: SD card image directory
The build SHALL provide a target that assembles two git-ignored directories:
- `build/sdcard/`: the universal SD card defined in `board-support`, with all SD-card kernel images, the boot files of both boot-firmware generations, the device trees, `armstub8-rpi4.bin`, the generated `config.txt`, all Wi-Fi firmware in `firmware/`, and the example configuration files.
- `build/cm4-emmc/`: the CM4 eMMC directory defined in `board-support`.

#### Scenario: Assemble SD card contents
- **WHEN** the developer runs the SD card target
- **THEN** `build/sdcard/` contains `kernel.img`, `kernel8.img`, `kernel8-rpi4.img`, `armstub8-rpi4.bin`, `config.txt`, `bootcode.bin`, `start.elf`, `fixup.dat`, `start4.elf`, `fixup4.dat`, the device trees, `firmware/` with the files for CYW43430, 43436, 43436s, 43455 and 43456, `raspir-blaster.conf.example` and `wpa_supplicant.conf.example`

#### Scenario: Assemble CM4 eMMC contents
- **WHEN** the developer runs the SD card target
- **THEN** `build/cm4-emmc/` contains the `rpi4-emmc` kernel image as `kernel8-rpi4.img`, `armstub8-rpi4.bin`, `config.txt`, `start4.elf`, `fixup4.dat`, the CM4 device tree, `firmware/` and the example configuration files

#### Scenario: No secrets in the output templates
- **WHEN** the SD card and eMMC directories are assembled
- **THEN** they contain no real Wi-Fi or MQTT credentials, only placeholder values in the example files

### Requirement: Host-side unit tests
Logic that does not depend on hardware (config parsing, IR message parsing and serialization, frame segmentation, PWM carrier calculation, discovery payload generation, reconnect backoff) SHALL be compilable with the host compiler. It SHALL be covered by unit tests run through one test command, without the cross toolchain.

#### Scenario: Run tests
- **WHEN** the developer runs the test command
- **THEN** all host unit tests are compiled with the host compiler and run, and the command exits non-zero if any test fails

### Requirement: Clean targets preserve the toolchain
The build SHALL offer a clean target that removes firmware build outputs but keeps `toolchain/`, and a separate target that also removes `toolchain/`.

#### Scenario: Regular clean
- **WHEN** the developer runs the regular clean target
- **THEN** the object directories and kernel images of all targets, `build/sdcard/` and `build/cm4-emmc/` are removed, and `toolchain/` (including the built Circle trees) is untouched

### Requirement: Circle is configured per build target
The build SHALL keep one Circle tree per build target under `toolchain/circle-<target>/`, derived from one pinned Circle checkout. Each tree SHALL be configured through its own `Config.mk`:

| Target      | `RASPPI` | `AARCH` | Prefix               | Extra defines           |
| ----------- | -------- | ------- | -------------------- | ----------------------- |
| `rpi0w`     | 1        | 32      | `arm-none-eabi-`     |                         |
| `rpi3`      | 3        | 64      | `aarch64-none-elf-`  |                         |
| `rpi4`      | 4        | 64      | `aarch64-none-elf-`  |                         |
| `rpi4-emmc` | 4        | 64      | `aarch64-none-elf-`  | `USE_EMBEDDED_MMC_CM`   |

Each tree SHALL build the Circle core libraries and the `SDCard`, `fatfs`, `wlan` and `wpa_supplicant` addons before the firmware of that target is linked.

#### Scenario: Circle configuration per target
- **WHEN** Circle is built by the bootstrap step for target `rpi4-emmc`
- **THEN** `toolchain/circle-rpi4-emmc/Config.mk` contains `RASPPI = 4`, `AARCH = 64`, a `PREFIX64` pointing to the downloaded `aarch64-none-elf-` toolchain, and the `USE_EMBEDDED_MMC_CM` define

#### Scenario: Targets do not share build artifacts
- **WHEN** two targets are built one after the other
- **THEN** neither the Circle libraries nor the firmware objects of one target are overwritten by the other

### Requirement: Pinned and verified boot and Wi-Fi firmware
The build SHALL download the Raspberry Pi boot files from the GitHub repository `raspberrypi/firmware` and the Wi-Fi firmware from `RPi-Distro/firmware-nonfree`, each at a commit hash recorded in the repository. Every downloaded file SHALL be checked against a SHA-256 checksum recorded in the repository. Downloads SHALL be cached under `toolchain/`.

#### Scenario: Pinned revisions
- **WHEN** the SD card target downloads boot or Wi-Fi files
- **THEN** the URLs reference the recorded commit hashes, not a branch

#### Scenario: Checksum mismatch
- **WHEN** a downloaded boot or Wi-Fi file does not match its recorded checksum
- **THEN** the SD card target aborts with an error naming the file, deletes the download, and does not assemble the output directories

#### Scenario: Cached downloads
- **WHEN** the SD card target runs a second time
- **THEN** no file is downloaded again

### Requirement: Build target selection
The top-level build SHALL accept a list of targets (e.g. `make TARGETS="rpi0w rpi4"`) and then download, configure and build only what those targets need. The default SHALL be all targets. The SD card target SHALL require that all targets placed into an output directory have been built.

#### Scenario: Single target
- **WHEN** the developer runs the build with `TARGETS=rpi3`
- **THEN** only the `rpi3` Circle tree and `kernel8.img` are built, and the `arm-none-eabi` toolchain is not required

#### Scenario: SD card with missing image
- **WHEN** the SD card target runs while a required image has not been built
- **THEN** it aborts with an error naming the missing target

### Requirement: Firmware version
The firmware version embedded in all kernel images SHALL be taken from, in this order:
1. the make variable `VERSION` (e.g. `make VERSION=2026.11.0`)
2. the file `VERSION` in the repository root, if it exists (only release branches have it)
3. the default `dev`

The version SHALL appear in the boot banner and as `sw_version` in Home Assistant discovery. When the version changes, the next build SHALL rebuild the affected objects, without a `make clean`. The host unit tests SHALL build without a version being set.

#### Scenario: Development build
- **WHEN** `make` runs on a branch without a `VERSION` file
- **THEN** the firmware reports the version `dev`

#### Scenario: Release branch build
- **WHEN** `make` runs on a branch whose `VERSION` file contains `2026.11.0`
- **THEN** all kernel images report the version `2026.11.0`

#### Scenario: Version change triggers rebuild
- **WHEN** `make` has built with version `dev` and then runs with `VERSION=2026.11.0`
- **THEN** the images report `2026.11.0` without a prior `make clean`

