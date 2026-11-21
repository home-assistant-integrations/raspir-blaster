## 1. Selection logic (src/core)

- [ ] 1.1 `src/core/netselect.{h,cpp}`: `NetSelect` and `NetSelectAfterProbe` as in design D1; add the header to `coreheaders.cpp`
- [ ] 1.2 `tests/test_netselect.cpp`: one test per row of the connectivity table, marker with and without `wpa_supplicant.conf`, and both probe paths (Pi 3: Wi-Fi, Pi 4: mark and reboot); `make test` passes

## 2. Firmware: configuration and Wi-Fi

- [ ] 2.1 `LoadConfig()`: record whether `wpa_supplicant.conf` exists instead of failing; keep the degraded path for `NetChoiceNone` with the existing message (D6)
- [ ] 2.2 Board capabilities from `CMachineInfo::GetMachineModel()` (`bHasEthernet`) and `RASPPI` (`bProbeBeforeNet`) (D2)
- [ ] 2.3 Split `InitializeNetwork()` into `InitializeWiFi()` (chip, net subsystem, WPA supplicant) and `InitializeEthernet()`; Wi-Fi chip only initialized for Wi-Fi (D5)

## 3. Firmware: Ethernet bring-up

- [ ] 3.1 `RASPPI == 3`: `CUSBHCIDevice` member, initialized only for Ethernet/probe; link probe helper (`UpdatePHY` + `IsLinkUp`, 100 ms steps, `ETHERNET_LINK_TIMEOUT_MS` = 5000) shared with Pi 4 (D3)
- [ ] 3.2 `RASPPI == 4`: `CNetSubSystem` on Ethernet with `Initialize (FALSE)`, then the probe for `NetChoiceProbe` (D4)
- [ ] 3.3 Marker file: check at boot, `f_unlink` before Wi-Fi bring-up, create + `CBcmWatchdog::Restart()` on `NetProbeMarkAndReboot` (D4)
- [ ] 3.4 Boot log lines of D7, including the link speed
- [ ] 3.5 `make` for all targets without warnings; `make sdcard` still succeeds; check image sizes against `main` (rpi0w must not grow by USB code)

## 4. Documentation

- [ ] 4.1 README: supported boards table with network column, CM4 without wireless supported, Ethernet selection (cable at boot, Pi 4/CM4 extra reboot, marker file), `wpa_supplicant.conf` optional on Ethernet boards, different IP per interface
- [ ] 4.2 `CHANGELOG.md` `[Unreleased]` entries (Added: Ethernet, CM4 without wireless; Changed: `wpa_supplicant.conf` optional on Ethernet boards)
- [ ] 4.3 `CLAUDE.md`: network selection model, Circle facts (net subsystem singleton, GENET inside the net layer, USB Ethernet on Pi 3)

## 5. Hardware verification (needs a Pi 3 B or B+ and a Pi 4 B or CM4)

- [ ] 5.1 Pi 4 B with cable and `wpa_supplicant.conf`: Ethernet, link speed logged, MQTT connected, IR send/receive work
- [ ] 5.2 Pi 4 B without cable: marker reboot, Wi-Fi, marker deleted; power-cycle with cable: Ethernet again
- [ ] 5.3 Pi 3 B or B+ with and without cable: Ethernet / Wi-Fi after ≤ 5 s, no reboot
- [ ] 5.4 Ethernet board without `wpa_supplicant.conf` and without cable: not degraded; plugging in the cable brings MQTT up
- [ ] 5.5 Pull and re-plug the cable at runtime: MQTT reconnects with backoff; record results in the hardware test report
