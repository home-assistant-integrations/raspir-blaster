## Context

`CKernel::InitializeNetwork()` (`src/fw/kernel.cpp`) always initializes the Wi-Fi chip (`CBcm4343Device`), creates `CNetSubSystem(..., NetDeviceTypeWLAN)` with `Initialize(FALSE)` and starts the WPA supplicant. `LoadConfig()` enters the degraded state if `wpa_supplicant.conf` is missing.

Relevant Circle facts (Step51.1.1, verified in `toolchain/circle`):
- `CNetSubSystem` is a singleton (`assert (s_pThis == 0)`) with one device type. `Initialize()` starts a `CNetTask` that is never stopped, and the destructor only clears `s_pThis`. **An initialized net subsystem cannot be replaced by one of another type.**
- `CNetDeviceLayer::Initialize()` initializes the GENET driver (`CBcm54213Device`, member of the layer) only on `RASPPI == 4`, regardless of the device type. **On Pi 4/CM4 the Ethernet link can only be seen after the net subsystem has been created.** The device is looked up lazily via `CNetDevice::GetNetDevice(TNetDeviceType)` in `Process()`.
- Pi 3 B / B+ Ethernet chips are USB devices (`CSMSC951xDevice`, `CLAN7800Device`). They register as `NetDeviceTypeEthernet` when the USB host controller (`CUSBHCIDevice`) enumerates them, independent of `CNetSubSystem`.
- All three drivers report the link through `IsLinkUp()`, refreshed by `UpdatePHY()` (normally called every 2 s by Circle's `CPHYTask`). Circle itself waits up to 4 s for the link in `Initialize(TRUE)`.
- `CBcmWatchdog::Restart()` reboots immediately. `libusb.a` is already linked into the firmware.

## Goals / Non-Goals

**Goals:**
- Ethernet on Pi 3 B/B+, Pi 4 B and CM4, chosen automatically by cable detection at boot (the maintainer's decision), with Wi-Fi as fallback.
- No regression on Wi-Fi-only boards, and no new configuration keys.
- Decision logic testable on the host.

**Non-Goals:**
- See proposal: no runtime switching, no dual-interface, no forcing key, no USB adapters on port-less boards, no static IP/IPv6.

## Decisions

### D1 — Pure selection logic in `src/core/netselect.{h,cpp}`
```
enum TNetChoice { NetChoiceWiFi, NetChoiceEthernet, NetChoiceProbe, NetChoiceNone };
TNetChoice NetSelect (bool bHasEthernet, bool bWiFiConfig, bool bWiFiMarker);
enum TNetProbeResult { NetProbeEthernet, NetProbeWiFi, NetProbeMarkAndReboot };
TNetProbeResult NetSelectAfterProbe (bool bLinkUp, bool bProbeBeforeNet);
```
`NetSelect` implements the table of the connectivity spec; `NetChoiceProbe` means "Ethernet board with Wi-Fi config and no marker". `NetSelectAfterProbe`: link up → Ethernet; no link and `bProbeBeforeNet` (Pi 3) → Wi-Fi; no link otherwise (Pi 4/CM4) → mark and reboot. A marker without `wpa_supplicant.conf` is ignored (Ethernet) and deleted. Host tests cover every table row and both probe paths.
*Alternative:* inline `if`s in `CKernel`. Rejected: the reboot path cannot be tested without hardware; the table can.

### D2 — Board capabilities from the machine model
In `src/fw`: `bHasEthernet` = model is `MachineModel3B`, `MachineModel3BPlus`, `MachineModel4B` or `MachineModelCM4`; `bProbeBeforeNet` = `RASPPI == 3`. The CM4 without wireless reports `MachineModelCM4` as well; its missing Wi-Fi chip only matters if Wi-Fi is chosen.

### D3 — Pi 3 B/B+: USB probe before the net subsystem
On `RASPPI == 3` the kernel gets a `CUSBHCIDevice` member. It is initialized only when `NetSelect` returns `NetChoiceProbe` or `NetChoiceEthernet` (so Zero 2 W and 3 A+ never start USB). The probe looks up `CNetDevice::GetNetDevice (NetDeviceTypeEthernet)` and calls `UpdatePHY()` + `IsLinkUp()` every 100 ms (with `MsSleep`, so other tasks run) until the link is up or 5 s have passed since USB enumeration finished. Then `CNetSubSystem` is created with the chosen type. If the chip is not found at all (broken board), the result counts as "no link".
On `RASPPI == 1` (Zero W) no USB member exists.

### D4 — Pi 4/CM4: start on Ethernet, marker file, one reboot
For `NetChoiceProbe` and `NetChoiceEthernet` on `RASPPI == 4`, the kernel creates `CNetSubSystem (..., NetDeviceTypeEthernet)` and calls `Initialize (FALSE)`. For `NetChoiceProbe` it then polls the GENET device as in D3 for up to 5 s. Without link it creates the empty file `SD:/raspir-blaster.wifi` (`f_open` with `FA_CREATE_ALWAYS | FA_WRITE`, `f_close`), logs the reason, and calls `CBcmWatchdog::Restart()`.
At the next boot `NetSelect` sees the marker, the kernel deletes it with `f_unlink` **before** bringing up Wi-Fi (so any later reboot, e.g. from the fatal state, tries Ethernet again), and uses Wi-Fi.
*Alternatives:*
- Read the PHY link directly via GENET MDIO registers before the net subsystem exists. Rejected: duplicates driver internals (register layout, PHY reset/power state after the VideoCore firmware), and cannot be verified without hardware or emulation (QEMU has no GENET).
- Keep a second, uninitialized `CNetSubSystem` around. Rejected: impossible, the class is a singleton and the GENET driver lives inside it.
- Pass the flag across the reboot in RAM or via the watchdog partition number. Rejected: RAM is not preserved reliably, and the partition number selects the boot partition.

### D5 — Wi-Fi only initialized for Wi-Fi
`m_WLAN.Initialize()` and `m_WPASupplicant.Initialize()` run only when Wi-Fi is chosen. This makes the CM4 without wireless work on Ethernet, and saves the Wi-Fi firmware load (~1 s) on Ethernet. The existing failure path stays: Wi-Fi chosen and the chip missing → "Cannot initialize Wi-Fi" → fatal.

### D6 — Configuration loading
`LoadConfig()` no longer fails on a missing `wpa_supplicant.conf`; it only records `bWiFiConfig`. The degraded state for "no Wi-Fi config" is reached through `NetChoiceNone` (board without Ethernet), with the same log message and LED pattern as today.

### D7 — Boot log
One line before network bring-up: `Network: Ethernet (link up, <speed>)`, `Network: Ethernet (no wpa_supplicant.conf)`, `Network: Wi-Fi (no Ethernet link)`, `Network: Wi-Fi (after Ethernet check)` or `Network: Wi-Fi (board has no Ethernet)`; plus `No Ethernet link, rebooting to use Wi-Fi` before the marker reboot. Speed via `CNetDevice::GetSpeedString (GetLinkSpeed ())`. The existing "Network up, IP address …" line stays.

## Risks / Trade-offs

- [No hardware or emulator for Ethernet here; QEMU has no GENET and no LAN95xx] → logic in `src/core` with tests; firmware builds for all targets; hardware tests listed as their own task group, to be run on a Pi 3 B/B+ and a Pi 4/CM4.
- [5 s may be too short for some switches (e.g. STP ports take 30 s to forward, but link is up earlier; PHY link usually comes in 2–3 s)] → only the PHY link is checked, not traffic; constant `ETHERNET_LINK_TIMEOUT_MS` in one place.
- [Reboot cycle on a CM4 without wireless that has `wpa_supplicant.conf` and no cable: marker reboot → Wi-Fi fails → fatal → watchdog reboot → …] → every second boot retries Ethernet, so plugging in a cable recovers; documented in README; same pattern as today's fatal state.
- [SD card writes: one create + one delete per boot pair without cable on Pi 4/CM4] → negligible wear; power loss between create and reboot leaves a marker, which only causes one Wi-Fi boot.
- [Different MAC address per interface → the DHCP server hands out a different IP] → README notes it; MQTT uses the broker address, so nothing else depends on the device IP.
- [USB enumeration adds ~1–2 s to boot on Pi 3 B/B+] → only on boards with Ethernet.

## Migration Plan

Existing installations keep working: boards without Ethernet behave as before; boards with Ethernet and `wpa_supplicant.conf` use Wi-Fi when no cable is connected (Pi 4/CM4 after one extra reboot). Rollback: flash the previous release; a leftover `raspir-blaster.wifi` file is ignored by older firmware.
