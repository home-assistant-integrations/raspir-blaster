# board-support Specification

## Purpose
TBD - created by archiving change add-multi-board-builds. Update Purpose after archive.
## Requirements
### Requirement: Supported boards and images
The firmware SHALL support the following boards, each through the listed build target and kernel image:

| Board                          | SoC      | Build target | Mode    | Image              | Boot media |
| ------------------------------ | -------- | ------------ | ------- | ------------------ | ---------- |
| Raspberry Pi Zero W            | BCM2835  | `rpi0w`      | AArch32 | `kernel.img`       | SD card    |
| Raspberry Pi Zero 2 W          | BCM2710A1| `rpi3`       | AArch64 | `kernel8.img`      | SD card    |
| Raspberry Pi 3 B / 3 B+ / 3 A+ | BCM2837  | `rpi3`       | AArch64 | `kernel8.img`      | SD card    |
| Raspberry Pi 4 B               | BCM2711  | `rpi4`       | AArch64 | `kernel8-rpi4.img` | SD card    |
| Compute Module 4 Lite          | BCM2711  | `rpi4`       | AArch64 | `kernel8-rpi4.img` | SD card    |
| Compute Module 4 with eMMC     | BCM2711  | `rpi4-emmc`  | AArch64 | `kernel8-rpi4.img` | eMMC       |

#### Scenario: Each target builds its image
- **WHEN** all targets are built
- **THEN** one kernel image exists per target, built with the Circle configuration of that target

#### Scenario: Same firmware features on every board
- **WHEN** the firmware runs on any supported board
- **THEN** IR receive, IR transmit, MQTT, Home Assistant discovery, configuration and watchdog behave as specified in the other capabilities

### Requirement: Universal SD card
One SD card directory SHALL boot every supported board that boots from an SD card (Zero W, Zero 2 W, Pi 3, Pi 4, CM4 Lite). It SHALL contain all kernel images for these boards, the boot files for both boot-firmware generations (`bootcode.bin`/`start.elf`/`fixup.dat` and `start4.elf`/`fixup4.dat`), the device trees the boot firmware needs, `armstub8-rpi4.bin`, and a generated `config.txt`. Using conditional sections, `config.txt` SHALL select per board the kernel image, `arm_64bit`, and (for Pi 4/CM4) the armstub.

#### Scenario: Zero W boots the 32-bit image
- **WHEN** the universal card is inserted into a Zero W
- **THEN** the boot firmware loads `kernel.img` in 32-bit mode and the firmware banner appears on the serial console

#### Scenario: Zero 2 W and Pi 3 boot the 64-bit image
- **WHEN** the universal card is inserted into a Zero 2 W or a Pi 3 B/B+/A+
- **THEN** the boot firmware loads `kernel8.img` with `arm_64bit=1`

#### Scenario: Pi 4 and CM4 Lite boot the Pi 4 image
- **WHEN** the universal card is inserted into a Pi 4 B or a CM4 Lite carrier board
- **THEN** the boot firmware loads `armstub8-rpi4.bin` and `kernel8-rpi4.img` with `arm_64bit=1`

### Requirement: CM4 eMMC image
The build SHALL produce a separate directory with the files for a CM4 with eMMC: the Pi 4 boot files, the `rpi4-emmc` kernel image (built with `USE_EMBEDDED_MMC_CM`), `armstub8-rpi4.bin`, a `config.txt` for this module, the Wi-Fi firmware and the example configuration files. The firmware in this image SHALL read its configuration and Wi-Fi firmware from the eMMC.

#### Scenario: CM4 with eMMC
- **WHEN** the directory is copied to the boot partition of a CM4's eMMC (e.g. via `rpiboot`) together with valid configuration files
- **THEN** the CM4 boots the firmware, reads `raspir-blaster.conf` from the eMMC and connects to Wi-Fi

#### Scenario: Wrong image on a CM4 with eMMC
- **WHEN** the universal SD card image is copied to a CM4's eMMC
- **THEN** the firmware cannot read its storage, logs the storage error and reboots through the watchdog (fatal state)

### Requirement: Identical pin assignment on all boards
On every supported board, the IR receiver SHALL be connected to GPIO23 (40-pin header pin 16) and the IR transmitter to GPIO18 (header pin 12, PWM0, alternate function 5). The serial log SHALL be on UART0 (GPIO14/15, header pins 8/10). On a CM4 the same GPIOs are used through the carrier board's 40-pin header.

#### Scenario: Moving the wiring to another board
- **WHEN** an IR receiver and transmitter wired for a Zero W are connected to the same header pins of a Pi 3 or Pi 4
- **THEN** receiving and transmitting work without configuration changes

### Requirement: Wi-Fi chip firmware
The SD card and eMMC directories SHALL contain the Wi-Fi firmware for every chip used by the supported boards. The firmware SHALL load the files for the chip detected at runtime.

| Chip     | Firmware files              | Boards                         |
| -------- | --------------------------- | ------------------------------ |
| CYW43430 | `brcmfmac43430-sdio.*`      | Zero W, 3 B                    |
| CYW43436 | `brcmfmac43436-sdio.*`, `brcmfmac43436s-sdio.*` | Zero 2 W   |
| CYW43455 | `brcmfmac43455-sdio.*`      | 3 B+, 3 A+, 4 B, CM4           |
| CYW43456 | `brcmfmac43456-sdio.*`      | later 4 B / CM4 revisions      |

#### Scenario: Board-specific chip
- **WHEN** the firmware starts on any supported board
- **THEN** the Wi-Fi driver finds the firmware for the detected chip in `firmware/` and joins the configured network

### Requirement: Board identification at runtime
At boot the firmware SHALL detect the board model and log it on the serial console, before the network is initialized.

#### Scenario: Boot log names the board
- **WHEN** the firmware boots on a Raspberry Pi 4 Model B
- **THEN** the serial log contains a line naming "Raspberry Pi 4 Model B" and the firmware version

### Requirement: Unsupported boards
Boards outside the supported list SHALL NOT be listed in the generated `config.txt`. The documentation SHALL name them as unsupported: Pi 5, CM5, Pi 400, Pi 1, Pi 2, Zero without W, CM3/CM3+, and CM4 without wireless.

#### Scenario: Pi 5
- **WHEN** the universal card is inserted into a Pi 5
- **THEN** no raspir-blaster kernel is started (the Pi 5 kernel image `kernel_2712.img` does not exist on the card)

#### Scenario: CM4 without wireless
- **WHEN** the firmware runs on a CM4 without a Wi-Fi chip
- **THEN** it logs "Cannot initialize Wi-Fi" and enters the fatal state

