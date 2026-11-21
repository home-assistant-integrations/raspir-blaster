## 1. Selection logic (src/core)

- [x] 1.1 `src/core/netselect.{h,cpp}`: `NetSelect` and `NetSelectAfterProbe` as in design D1; add the header to `coreheaders.cpp`
- [x] 1.2 `tests/test_netselect.cpp`: one test per row of the connectivity table, marker with and without `wpa_supplicant.conf`, and both probe paths (Pi 3: Wi-Fi, Pi 4: mark and reboot); `make test` passes

## 2. Firmware: configuration and Wi-Fi

- [x] 2.1 `LoadConfig()`: record whether `wpa_supplicant.conf` exists instead of failing; keep the degraded path for `NetChoiceNone` with the existing message (D6)
- [x] 2.2 Board capabilities from `CMachineInfo::GetMachineModel()` (`bHasEthernet`) and `RASPPI` (`bProbeBeforeNet`) (D2)
- [x] 2.3 Split `InitializeNetwork()` into `InitializeWiFi()` (chip, net subsystem, WPA supplicant) and `InitializeEthernet()`; Wi-Fi chip only initialized for Wi-Fi (D5)

## 3. Firmware: Ethernet bring-up

- [x] 3.1 `RASPPI == 3`: `CUSBHCIDevice` member, initialized only for Ethernet/probe; link probe helper (`UpdatePHY` + `IsLinkUp`, 100 ms steps, `ETHERNET_LINK_TIMEOUT_MS` = 5000) shared with Pi 4 (D3)
- [x] 3.2 `RASPPI == 4`: `CNetSubSystem` on Ethernet with `Initialize (FALSE)`, then the probe for `NetChoiceProbe` (D4)
- [x] 3.3 Marker file: check at boot, `f_unlink` before Wi-Fi bring-up, create + `CBcmWatchdog::Restart()` on `NetProbeMarkAndReboot` (D4)
- [x] 3.4 Boot log lines of D7, including the link speed
- [x] 3.5 `make` for all targets without warnings; `make sdcard` still succeeds; check image sizes against `main` (rpi0w must not grow by USB code)

## 4. Documentation

- [x] 4.1 README: supported boards table with network column, CM4 without wireless supported, Ethernet selection (cable at boot, Pi 4/CM4 extra reboot, marker file), `wpa_supplicant.conf` optional on Ethernet boards, different IP per interface
- [x] 4.2 `CHANGELOG.md` `[Unreleased]` entries (Added: Ethernet, CM4 without wireless; Changed: `wpa_supplicant.conf` optional on Ethernet boards)
- [x] 4.3 `CLAUDE.md`: network selection model, Circle facts (net subsystem singleton, GENET inside the net layer, USB Ethernet on Pi 3)

## 5. Hardware verification

Moved to the change `verify-ir-blaster-hardware` (group 8, requirement "Ethernet selection and operation"), so that all hardware tests live in one change.

Pre-check in QEMU 11.1.2 (2026-11-21, not a substitute for the hardware tests in `verify-ir-blaster-hardware`, group 8). Test build = this firmware with the three `m_Watchdog.Start()` calls removed, because QEMU's `bcm2835-powermgt` resets immediately when the watchdog is armed. Card: 128 MiB FAT32 image via `-drive if=sd`.
- `raspi3b` + `usb-net` (USB CDC Ethernet), no `wpa_supplicant.conf`: "Network: Ethernet (no SD:/wpa_supplicant.conf)", USB enumerates the device, IR TX initialized, DHCP DISCOVER sent and answered by QEMU (pcap), but the reply never reaches the firmware (known receive bug of stock QEMU's `usb-net`, Circle's `doc/qemu.txt` recommends a patched QEMU); DHCP retries with backoff.
- `raspi3b` + `usb-net`, with `wpa_supplicant.conf`: link check → "Network: Ethernet (link up)" (the speed is omitted when the device reports none: fixed after this run, it printed "Unknown"); Wi-Fi not initialized.
- `raspi3b` without network device, with `wpa_supplicant.conf`: "Ethernet chip not found" → "Network: Wi-Fi (no Ethernet)" → "Cannot initialize Wi-Fi" (QEMU has no Wi-Fi chip) → fatal.
- `raspi3b`, marker `raspir-blaster.wifi` + `wpa_supplicant.conf`: "Network: Wi-Fi (after Ethernet check)"; the marker was deleted from the card image, other files kept.
- `raspi3b`, no `raspir-blaster.conf`: degraded state. `raspi3b` stock image (watchdog armed): fatal path resets QEMU immediately.
- `raspi0` (rpi0w image via `-bios`): with `wpa_supplicant.conf` "Network: Wi-Fi (board has no Ethernet)"; without it "Wi-Fi configuration missing" → degraded.
- `raspi4b`: not testable; Circle's EMMC2 init fails at once ("Cannot initialize SD card"), and QEMU emulates no GENET, so the marker reboot path could not be run.
