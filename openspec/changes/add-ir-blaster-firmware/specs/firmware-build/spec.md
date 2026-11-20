## ADDED Requirements

### Requirement: One-command build on macOS
The project SHALL provide one top-level build command, run from the repository root on a macOS host (Apple Silicon or Intel), that produces the firmware image `kernel.img` for the BCM2835 (`RASPPI=1`, `AARCH=32`).

#### Scenario: First build on a clean checkout
- **WHEN** the developer runs the top-level build command on a fresh clone without a `toolchain/` directory
- **THEN** the build downloads all dependencies, builds Circle and its addons, and produces `kernel.img`, all without manual steps

#### Scenario: Incremental build
- **WHEN** the developer changes a firmware source file and runs the build command again
- **THEN** only the affected firmware objects are rebuilt, and neither the toolchain nor Circle is downloaded or rebuilt again

### Requirement: Toolchain and Circle are downloaded into `toolchain/`
The build SHALL download the ARM GNU toolchain (`arm-none-eabi`, macOS build matching the host architecture) and the Circle sources into the git-ignored `toolchain/` directory. Both SHALL be pinned to explicit versions recorded in the repository. Toolchain archives SHALL be checked against a recorded SHA-256 checksum.

#### Scenario: Pinned versions
- **WHEN** the toolchain and Circle are downloaded
- **THEN** the exact toolchain release and Circle tag recorded in the repository are used

#### Scenario: Checksum mismatch
- **WHEN** a downloaded toolchain archive does not match its recorded SHA-256 checksum
- **THEN** the build aborts with an error naming the archive, and nothing is extracted into `toolchain/`

#### Scenario: Nothing downloaded outside `toolchain/`
- **WHEN** the build completes
- **THEN** all downloaded third-party content resides under `toolchain/`, and `git status` shows no untracked third-party files

### Requirement: Circle is configured for the BCM2835
The build SHALL configure Circle with `RASPPI = 1`, `AARCH = 32` and the `arm-none-eabi-` prefix. It SHALL build the Circle core libraries and the `SDCard`, `fatfs`, `wlan` and `wpa_supplicant` addons before linking the firmware.

#### Scenario: Circle configuration
- **WHEN** Circle is built by the bootstrap step
- **THEN** its `Config.mk` contains `RASPPI = 1`, `AARCH = 32` and a `PREFIX` that points to the downloaded `arm-none-eabi-` toolchain

### Requirement: SD card image directory
The build SHALL provide a target that assembles a directory containing everything to copy onto a FAT32 SD card. The directory SHALL hold the Raspberry Pi boot firmware (`bootcode.bin`, `start.elf`, `fixup.dat`), `kernel.img`, the CYW43438 Wi-Fi firmware (`brcmfmac43430-sdio.bin`, `brcmfmac43430-sdio.txt`) in a `firmware/` subdirectory, and example configuration files. The output directory SHALL be git-ignored.

#### Scenario: Assemble SD card contents
- **WHEN** the developer runs the SD card target
- **THEN** the output directory contains the boot firmware, `kernel.img`, `firmware/brcmfmac43430-sdio.*`, `raspir-blaster.conf.example` and `wpa_supplicant.conf.example`

#### Scenario: No secrets in the output templates
- **WHEN** the SD card directory is assembled
- **THEN** it contains no real Wi-Fi or MQTT credentials, only placeholder values in the example files

### Requirement: Host-side unit tests
Logic that does not depend on hardware (config parsing, IR message parsing and serialization, frame segmentation, PWM carrier calculation, discovery payload generation, reconnect backoff) SHALL be compilable with the host compiler. It SHALL be covered by unit tests run through one test command, without the cross toolchain.

#### Scenario: Run tests
- **WHEN** the developer runs the test command
- **THEN** all host unit tests are compiled with the host compiler and run, and the command exits non-zero if any test fails

### Requirement: Clean targets preserve the toolchain
The build SHALL offer a clean target that removes firmware build outputs but keeps `toolchain/`, and a separate target that also removes `toolchain/`.

#### Scenario: Regular clean
- **WHEN** the developer runs the regular clean target
- **THEN** firmware objects, `kernel.img` and the SD card directory are removed, and `toolchain/` is untouched
