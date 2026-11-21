## MODIFIED Requirements

### Requirement: SD card image directory
The build SHALL provide a target that assembles two git-ignored directories:
- `build/sdcard/`: the universal SD card defined in `board-support`, with all SD-card kernel images, the boot files of both boot-firmware generations, the device trees, `armstub8-rpi4.bin`, the generated `config.txt`, all Wi-Fi firmware in `firmware/`, the example configuration files, and the update manifest.
- `build/cm4-emmc/`: the CM4 eMMC directory defined in `board-support`, with its update manifest.

The update manifest `raspir-blaster-manifest.txt` SHALL be a text file with one `key=value` per line: `version=<firmware version>`, `build=sdcard` or `build=cm4-emmc`, and one `sha256.<image>=<64 hex digits>` line per kernel image in the directory.

#### Scenario: Assemble SD card contents
- **WHEN** the developer runs the SD card target
- **THEN** `build/sdcard/` contains `kernel.img`, `kernel8.img`, `kernel8-rpi4.img`, `armstub8-rpi4.bin`, `config.txt`, `bootcode.bin`, `start.elf`, `fixup.dat`, `start4.elf`, `fixup4.dat`, the device trees, `firmware/` with the files for CYW43430, 43436, 43436s, 43455 and 43456, `raspir-blaster.conf.example`, `wpa_supplicant.conf.example` and `raspir-blaster-manifest.txt`

#### Scenario: Assemble CM4 eMMC contents
- **WHEN** the developer runs the SD card target
- **THEN** `build/cm4-emmc/` contains the `rpi4-emmc` kernel image as `kernel8-rpi4.img`, `armstub8-rpi4.bin`, `config.txt`, `start4.elf`, `fixup4.dat`, the CM4 device tree, `firmware/`, the example configuration files and `raspir-blaster-manifest.txt`

#### Scenario: No secrets in the output templates
- **WHEN** the SD card and eMMC directories are assembled
- **THEN** they contain no real Wi-Fi or MQTT credentials, only placeholder values in the example files

#### Scenario: Manifest content
- **WHEN** `make VERSION=2026.12.0 sdcard` runs
- **THEN** `build/sdcard/raspir-blaster-manifest.txt` contains `version=2026.12.0`, `build=sdcard` and the SHA-256 of `kernel.img`, `kernel8.img` and `kernel8-rpi4.img`, each matching `shasum -a 256` of the file
