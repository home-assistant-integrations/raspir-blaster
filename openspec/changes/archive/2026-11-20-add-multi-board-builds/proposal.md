## Why

The firmware currently builds only for the Raspberry Pi Zero W (BCM2835). Raspberry Pi Zero 2 W, Pi 3 and Pi 4 / CM4 boards are common and have the same 40-pin GPIO layout and on-board Wi-Fi. Supporting them lets users run the IR blaster on whatever board they have. All these boards use Broadcom SoCs with the same PWM, GPIO and watchdog blocks, so the existing firmware needs only small, contained changes. Most of the work is in the build.

## What Changes

- Build four firmware images in one run, each against its own Circle configuration:

  | Target      | Boards                                 | Circle config            | Image              |
  | ----------- | -------------------------------------- | ------------------------ | ------------------ |
  | `rpi0w`     | Zero W                                 | `RASPPI=1`, `AARCH=32`   | `kernel.img`       |
  | `rpi3`      | Zero 2 W, 3B, 3B+, 3A+                 | `RASPPI=3`, `AARCH=64`   | `kernel8.img`      |
  | `rpi4`      | 4B, CM4 Lite (SD card)                 | `RASPPI=4`, `AARCH=64`   | `kernel8-rpi4.img` |
  | `rpi4-emmc` | CM4 with eMMC                          | `RASPPI=4`, `AARCH=64`, `USE_EMBEDDED_MMC_CM` | `kernel8-rpi4.img` |

- Download a second toolchain, ARM GNU Toolchain `aarch64-none-elf` (same release, `darwin-arm64`, SHA-256 verified), next to the existing `arm-none-eabi` toolchain.
- Download all boot files and Wi-Fi firmware for these boards directly from the Raspberry Pi GitHub repositories, at commit hashes pinned in the repository. Every file is checked against a recorded SHA-256 checksum.
  - `raspberrypi/firmware`: `bootcode.bin`, `start*.elf`, `fixup*.dat`, the device trees, the licence.
  - `RPi-Distro/firmware-nonfree`: the CYW43430, 43436, 43436s, 43455 and 43456 firmware.
- **BREAKING (build output):** `make sdcard` produces two directories:
  - `build/sdcard/`: one universal SD card for Zero W, Zero 2 W, Pi 3, Pi 4 and CM4 Lite. A generated `config.txt` selects the image and 32/64-bit mode per board.
  - `build/cm4-emmc/`: the files to flash onto a CM4's eMMC.
- Firmware changes needed for the new SoCs:
  - The PWM carrier calculation uses the SoC's oscillator: 19.2 MHz on BCM2835/BCM2837, 54 MHz on BCM2711.
  - The Home Assistant device model reports the detected board instead of a fixed "Raspberry Pi Zero W" string.
- Allow building a subset of targets (e.g. `make TARGETS=rpi4`) for faster development builds.

## Capabilities

### New Capabilities
- `board-support`: the supported boards, the board-to-image mapping, identical pin assignment on all boards, supported Wi-Fi chips, runtime board detection, and which board variants are explicitly unsupported.

### Modified Capabilities
- `firmware-build`:
  - The one-command build produces all four images.
  - Circle is configured per target instead of only for the BCM2835.
  - A second toolchain is downloaded.
  - Boot and Wi-Fi firmware downloads are pinned and verified.
  - The SD card target produces the universal card and the CM4 eMMC directory.
  - A target subset can be selected.
- `connectivity`: Wi-Fi firmware for the detected chip (not only the CYW43438) is loaded; the SoC watchdog replaces the "BCM2835 watchdog" wording.
- `ir-transmit`: the hardware PWM carrier is derived from the SoC-specific oscillator.
- `ha-discovery`: the device `model` reflects the detected board.

## Non-goals

- **Raspberry Pi 5 and CM5.** Their GPIO, PWM and clocks are in the RP1 chip, which needs different drivers.
- **Pi 1, Pi 2, Pi Zero (non-W), Pi 3 CM3/CM3+.** No on-board Wi-Fi, or not requested.
- **CM4 variants without wireless.** Ethernet is still out of scope, so these boards have no network.
- **Pi 400.** Not requested and not tested. It may work with the `rpi4` image but is not a supported board.
- **32-bit images for 64-bit-capable boards.**
- **USB boot or network boot.** Boot media stays SD card, or eMMC on CM4.
- **Changes to the IR, MQTT or configuration behavior** beyond the SoC-specific carrier clock and the model string.

## Impact

- **Build:**
  - `scripts/bootstrap.sh`: second toolchain, four Circle trees `toolchain/circle-<target>/` made as APFS clones of one pristine checkout.
  - `scripts/versions.env`: aarch64 toolchain checksum, firmware repo commits.
  - New checksum list for the boot and Wi-Fi files.
  - `src/fw/Makefile`: out-of-tree object directories `build/obj/<target>/`.
  - `scripts/sdcard.sh`: universal card, `config.txt` generation, CM4 eMMC directory.
  - Top-level `Makefile`: `TARGETS` variable.
- **Firmware:** `src/core/ircarrier` takes the oscillator frequency as a parameter; `src/core/discovery` takes the model string; `CKernel` passes `CMachineInfo` data.
- **Disk/time:** `toolchain/` grows by about 150 MB for the second toolchain. The first build compiles Circle four times, a few minutes on Apple Silicon.
- **Docs:** README (supported boards, SD card vs. CM4 eMMC flashing), `CLAUDE.md`, design doc.
- **Hardware verification:** the open change `verify-ir-blaster-hardware` covers only the Zero W. Other boards need their own test runs later.
