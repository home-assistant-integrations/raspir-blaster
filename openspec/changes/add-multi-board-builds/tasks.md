## 1. Pinned versions and manifests

- [x] 1.1 Add the `aarch64-none-elf` toolchain (archive name, SHA-256 for `darwin-arm64`) and `RPI_FIRMWARE_COMMIT` / `RPI_WIFI_FIRMWARE_COMMIT` (initially Circle Step51.1.1's pins) to `scripts/versions.env`
- [x] 1.2 Create `scripts/firmware-files.txt` listing every boot and Wi-Fi file from design D5 with repository, path, destination name and output directories (checksums still empty)
- [x] 1.3 Add `scripts/firmware-checksums.sh` and the `make firmware-checksums` target: download every manifest file at the pinned commit and write its SHA-256 into the manifest; run it once and commit the filled-in manifest

## 2. Bootstrap per target

- [x] 2.1 `bootstrap.sh`: accept a target list (default all four), map each target to `RASPPI`/`AARCH`/toolchain/extra defines, and fail with a clear error for unknown targets
- [x] 2.2 `bootstrap.sh`: download and verify only the toolchains the selected targets need (refactor `install_toolchain` to take the target triple)
- [x] 2.3 `bootstrap.sh`: create `toolchain/circle-<target>/` as an APFS clone of `toolchain/circle/` (`cp -Rc`, fallback `cp -R`) and write its `Config.mk` (`PREFIX` or `PREFIX64`, `USE_EMBEDDED_MMC_CM` for `rpi4-emmc`)
- [x] 2.4 `bootstrap.sh`: build core libraries and addons per target, guarded by per-target stamp files; check that a second run is a no-op
- [x] 2.5 `bootstrap.sh`: build `armstub8-rpi4.bin` in the `rpi4` tree when `rpi4` or `rpi4-emmc` is selected

## 3. Out-of-tree firmware builds

- [x] 3.1 `src/fw/Makefile`: take `SRCROOT`/`CIRCLEHOME` from the caller, use `vpath` and `EXTRAINCLUDE` for `src/fw` and `src/core`, and keep the `WATCHDOG_TEST` switch
- [x] 3.2 Top-level `Makefile`: `TARGETS` variable, a per-target loop building in `build/obj/<target>/` and copying the image to `build/images/<target>/`; pass `TARGETS` to the bootstrap
- [x] 3.3 Update the `clean`/`distclean` targets and `.gitignore` (remove the `src/fw` image entries); check that `make TARGETS=rpi0w` produces the same `kernel.img` as before

## 4. Firmware changes

- [x] 4.1 Tests + implementation: `IRCarrierCompute` with an oscillator parameter; tests for 19.2 MHz and 54 MHz (range 253/711 at 38 kHz, ±1 % and 33 % ± 3 % over 20–60 kHz)
- [x] 4.2 `CIRTransmitter`: pass the oscillator for `RASPPI` (19.2 MHz for ≤ 3, 54 MHz for 4)
- [x] 4.3 Tests + implementation: `DiscoveryBuildPayload` with a model parameter; update the expected payload and the longest-payload test ("Raspberry Pi 3 Model B+ (Circle bare-metal)", "Compute Module 4 (Circle bare-metal)")
- [x] 4.4 `CKernel`: log the board name from `CMachineInfo` after the banner and pass "<name> (Circle bare-metal)" to the MQTT client for discovery
- [x] 4.5 Build all four targets and fix any AArch64 warnings (format strings, integer widths); zero warnings on every target

## 5. SD card and eMMC directories

- [x] 5.1 Add the tracked `config/config.txt` (universal card) and `config/config-cm4-emmc.txt` from design D6
- [x] 5.2 `sdcard.sh`: download the manifest files at the pinned commits into `toolchain/downloads/`, verify SHA-256 (delete and abort on mismatch), skip cached files
- [x] 5.3 `sdcard.sh`: assemble `build/sdcard/` (images of `rpi0w`, `rpi3`, `rpi4`, armstub, boot files, device trees, `config.txt`, `firmware/`, example configs); abort if a required image is missing
- [x] 5.4 `sdcard.sh`: assemble `build/cm4-emmc/` (`rpi4-emmc` image as `kernel8-rpi4.img`, armstub, Pi 4 boot files, CM4 device tree, `config.txt`, `firmware/`, example configs)
- [x] 5.5 Check: a corrupted cached file makes `make sdcard` fail with the file name; a second `make sdcard` downloads nothing

## 6. Verification and documentation

- [x] 6.1 Clean-checkout build in a scratch copy (`make`, `make test`, `make sdcard`), with the expected file list of both output directories and `git status` free of third-party files
- [x] 6.2 README: supported board table (with "verified"/"built, not yet verified"), unsupported boards, SD card vs. CM4 eMMC (`rpiboot`) setup, `TARGETS`
- [x] 6.3 Update `CLAUDE.md` (targets, build layout, per-target Circle trees) and the design doc's board/SoC notes
- [x] 6.4 Add a note to the open change `verify-ir-blaster-hardware` that its tests currently cover the Zero W only, and which boards still need a run
