## Context

Today the build makes one image for one board:
- `scripts/bootstrap.sh` downloads one toolchain (`arm-none-eabi`) and one Circle checkout configured for `RASPPI=1`.
- `src/fw/Makefile` builds `kernel.img` inside `src/fw/`.
- `scripts/sdcard.sh` downloads the BCM2835 boot files and the CYW43430 Wi-Fi firmware, using the revisions pinned in Circle's own Makefiles.

Circle builds its libraries in place, inside its source tree, for exactly one `RASPPI`/`AARCH` combination. So several targets cannot share one Circle tree.

Boards in scope: Zero W, Zero 2 W, Pi 3 (B/B+/A+), Pi 4 B, CM4 Lite, CM4 with eMMC. All use Broadcom SoCs (BCM2835, BCM2710A1/BCM2837, BCM2711) with the same PWM, GPIO, GPCLK and watchdog peripherals, which Circle supports through the same classes. Pi 5/CM5 (RP1) are out of scope.

## Goals / Non-Goals

**Goals:**
- One `make` builds images for all boards, and `make sdcard` produces a universal SD card plus a CM4 eMMC directory.
- Every downloaded file (toolchains, boot files, Wi-Fi firmware) is pinned and checksum-verified.
- The firmware runs unchanged on every supported board, apart from SoC-specific constants.

**Non-Goals:**
- Pi 5/CM5, Pi 400, boards without Wi-Fi, 32-bit images for 64-bit boards, USB/network boot (see proposal).

## Decisions

### D1 — Four build targets
| Target      | Boards                   | `RASPPI` | `AARCH` | Toolchain          | Image              |
| ----------- | ------------------------ | -------- | ------- | ------------------ | ------------------ |
| `rpi0w`     | Zero W                   | 1        | 32      | `arm-none-eabi`    | `kernel.img`       |
| `rpi3`      | Zero 2 W, 3 B/B+/A+      | 3        | 64      | `aarch64-none-elf` | `kernel8.img`      |
| `rpi4`      | 4 B, CM4 Lite            | 4        | 64      | `aarch64-none-elf` | `kernel8-rpi4.img` |
| `rpi4-emmc` | CM4 with eMMC            | 4        | 64      | `aarch64-none-elf` | `kernel8-rpi4.img` |

64-bit mode is used wherever the SoC supports it; the Zero W's ARMv6 core supports only 32 bit. The CM4 needs two targets because Circle chooses between the SD card interface and the embedded MMC at compile time (`USE_EMBEDDED_MMC_CM`), and the firmware must read its configuration and Wi-Fi firmware from the boot storage.
*Alternative:* 32 bit everywhere (one toolchain, `kernel8-32.img`/`kernel7l.img`). Rejected by the project owner in favor of 64-bit images.

### D2 — One pristine Circle checkout, one APFS clone per target
`bootstrap.sh` keeps the existing `toolchain/circle/` checkout (with the hostap submodule) as a read-only source. For each selected target it creates `toolchain/circle-<target>/` with `cp -Rc`, an APFS clone: instant, and it uses no extra disk space until files change. It then writes that tree's `Config.mk` and builds the core libraries and addons there. Each target has its own stamp files (`.stamp-libs-<target>-<circle tag>-<toolchain>`). If the filesystem does not support clones, it falls back to `cp -R`.
*Alternatives:*
- Rebuild one tree per target in sequence: needs a full Circle clean and rebuild whenever the target changes, which makes incremental builds of several targets impossible.
- `git worktree`: the hostap submodule would have to be initialized again in every worktree.

### D3 — Out-of-tree firmware objects
The firmware is built in `build/obj/<target>/`. The top-level Makefile runs, for every target in `TARGETS`:
```
make -C build/obj/<target> -f $(ROOT)/src/fw/Makefile CIRCLEHOME=$(ROOT)/toolchain/circle-<target> SRCROOT=$(ROOT)
```
`src/fw/Makefile` uses `vpath %.cpp $(SRCROOT)/src/fw $(SRCROOT)/src/core` and `EXTRAINCLUDE = -I $(SRCROOT)/src/fw -I $(SRCROOT)/src/core`, so Circle's pattern rules find the sources while writing objects into the current directory. The resulting image is copied to `build/images/<target>/`. `TARGETS` defaults to `rpi0w rpi3 rpi4 rpi4-emmc`.
*Alternative:* one copy of `src/fw` per target. Rejected because it duplicates sources.

### D4 — Toolchains on demand
`scripts/versions.env` gets the `aarch64-none-elf` archive name and SHA-256 next to the existing `arm-none-eabi` entry (same release 15.2.rel1, `darwin-arm64`). `bootstrap.sh` downloads a toolchain only when a selected target needs it. `Config.mk` uses `PREFIX` for AArch32 and `PREFIX64` for AArch64, as Circle expects.

### D5 — Boot and Wi-Fi files: pinned commits plus a checksum manifest
`scripts/versions.env` pins `RPI_FIRMWARE_COMMIT` (repository `raspberrypi/firmware`) and `RPI_WIFI_FIRMWARE_COMMIT` (repository `RPi-Distro/firmware-nonfree`). The initial values are the revisions that Circle `Step51.1.1` pins, because that combination is tested with Circle. A tracked manifest `scripts/firmware-files.txt` lists one file per line: SHA-256, repository, path in the repository, destination name, and the output directories it belongs to (`sdcard`, `cm4-emmc`). `sdcard.sh` downloads each file at the pinned commit into `toolchain/downloads/<repo>-<commit>/` and checks its SHA-256. On a mismatch it deletes the file and aborts. A helper target `make firmware-checksums` regenerates the hashes after the commits are bumped on purpose.

Files (taken from Circle's boot and Wi-Fi download lists, restricted to the supported boards):
- **Boot:** `bootcode.bin`, `start.elf`, `fixup.dat` (Zero W, Zero 2 W, Pi 3); `start4.elf`, `fixup4.dat` (Pi 4/CM4); `bcm2710-rpi-zero-2-w.dtb`, `bcm2711-rpi-4-b.dtb`, `bcm2711-rpi-cm4.dtb`; `LICENCE.broadcom`.
- **Wi-Fi:** `brcmfmac43430-sdio.{bin,txt,clm_blob}`, `brcmfmac43436-sdio.{bin,txt,clm_blob}`, `brcmfmac43436s-sdio.{bin,txt}`, `brcmfmac43455-sdio.{bin,txt,clm_blob}` (the `cyfmac43455-sdio-minimal.bin` variant, as in Circle), `brcmfmac43456-sdio.{bin,txt,clm_blob}`.

*Alternative:* keep reading the revisions from Circle's Makefiles. Rejected: the project owner wants the files fetched explicitly from the Raspberry Pi repositories, and per-file checksums need an explicit list anyway.

### D6 — Tracked `config.txt` files with board filters
Two tracked files are copied verbatim:
- `config/config.txt` for the universal SD card.
- `config/config-cm4-emmc.txt` for the eMMC directory, copied there as `config.txt`.

The universal file uses Raspberry Pi conditional filters:
```
initial_turbo=0

[pi0w]
arm_64bit=0
kernel=kernel.img

[pi02]
arm_64bit=1
kernel_address=0x80000
kernel=kernel8.img

[pi3]
arm_64bit=1
kernel_address=0x80000
kernel=kernel8.img

[pi3+]
arm_64bit=1
kernel_address=0x80000
kernel=kernel8.img

[pi4]
arm_64bit=1
kernel_address=0x80000
armstub=armstub8-rpi4.bin
kernel=kernel8-rpi4.img
max_framebuffers=2

[cm4]
otg_mode=1

[all]
```
`[pi4]` also matches CM4 (Lite). Pi 5 has no section, and its default image `kernel_2712.img` is not on the card, so it does not boot. The eMMC file contains only the common lines plus the Pi 4 and CM4 sections.
*Alternative:* generate `config.txt` in `sdcard.sh`. Rejected because a tracked file is easier to review and to test.

### D7 — `armstub8-rpi4.bin`
The armstub is built from Circle's `boot/armstub` sources with the `aarch64-none-elf` toolchain (`make armstub8-rpi4.bin`) inside `toolchain/circle-rpi4/`. It is copied to both output directories.

### D8 — Firmware changes stay minimal
- `src/core/ircarrier`: `IRCarrierCompute(nFreqHz, nOscillatorHz, pCarrier)`. The fixed `IR_PWM_OSCILLATOR_HZ` is removed. `src/fw/irtransmitter` passes `19200000` for `RASPPI <= 3` and `54000000` for `RASPPI == 4`. The divider stays 2, giving a 9.6 MHz or 27 MHz PWM clock and a range of 253 or 711 at 38 kHz. Host tests cover both oscillators.
- `src/core/discovery`: `DiscoveryBuildPayload (…, pModel, …)`. `CKernel` passes `CMachineInfo::Get ()->GetMachineName ()` plus `" (Circle bare-metal)"`. The test for the longest payload uses the longest supported name.
- `CKernel::Initialize` logs the board name right after the banner.
- Code review for AArch64: no pointer/`unsigned` casts, all format strings match their argument types, and `CTimer::GetClockTicks()` arithmetic stays 32-bit.

### D9 — Build output layout
```
build/obj/<target>/        firmware objects per target
build/images/<target>/     kernel image per target
build/sdcard/              universal SD card
build/cm4-emmc/            CM4 eMMC boot files
build/tests/               host tests (unchanged)
```
`src/fw/` no longer receives build outputs, so its `.gitignore` entries are removed.

## Risks / Trade-offs

- **[Boards other than the Zero W are not tested on hardware]** → The images are built and the `config.txt` filters follow the Raspberry Pi documentation. Hardware runs are added to the verification change per board; until then, the README marks those boards as "built, not yet verified".
- **[Wrong `config.txt` filter semantics]** (e.g. whether `[pi3]` already covers 3+) → Explicit sections for `[pi02]`, `[pi3]` and `[pi3+]`. Later sections only repeat identical settings, so overlaps are harmless.
- **[CM4 eMMC flashing needs extra tooling]** (`rpiboot` / usbboot) → Documented in the README; the build only produces the files.
- **[A Zero 2 W with the CYW43436s chip variant]** → Both 43436 and 43436s files are on the card; Circle's driver chooses at runtime.
- **[First build time ×4]** (Circle is built four times) → Parallel `make -j`, stamp files, and `TARGETS=` for development builds.
- **[APFS clone not available]** (e.g. build tree on an external exFAT drive) → Falls back to `cp -R` (about 80 MB per target).
- **[Pinned firmware revisions drift away from what Circle tested]** → Defaults are Circle's pins. Bumping them is a deliberate change that needs `make firmware-checksums` and a hardware test.

## Migration Plan

1. The first `make` after this change downloads the aarch64 toolchain and builds the additional Circle trees. The existing `toolchain/circle/` stays as the pristine source.
2. Image paths change from `src/fw/kernel.img` to `build/images/<target>/`. Existing Zero W SD cards should be rewritten from the new `build/sdcard/`, because `config.txt` is new.
3. Rollback: check out the previous commit and run `make distclean && make`.

## Open Questions

- Do Pi 3 B/B+/A+ need their device tree files (`bcm2710-rpi-3-b*.dtb`) for Circle? Circle's download list contains only the Zero 2 W device tree among the BCM2710 boards, so they are not included. To be confirmed on hardware.
- Should the hardware verification change get one test matrix with a column per board, or one change per board? To be decided when the first non-Zero-W board is available.
