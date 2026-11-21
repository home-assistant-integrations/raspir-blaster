## Why

The firmware only connects over Wi-Fi, although the Pi 3 B, Pi 3 B+, Pi 4 B and every Compute Module 4 have an Ethernet port (on the CM4 the PHY is on the module, the jack on the carrier board). A wired connection is more reliable for a device that sits next to a TV for years, it does not need Wi-Fi credentials on the SD card, and it makes the CM4 variants without a Wi-Fi chip usable at all (today they end in the fatal state).

## What Changes

- **Ethernet on boards with a port:** Pi 3 B (USB chip LAN9514), Pi 3 B+ (USB chip LAN7515), Pi 4 B and CM4 (on-chip GENET with BCM54213/BCM54210 PHY), using Circle's existing drivers. DHCP host name, MQTT, discovery and all IR functions work the same as over Wi-Fi.
- **Automatic selection by cable detection at boot:** on a board with Ethernet, the firmware uses Ethernet if the link comes up within 5 s, otherwise Wi-Fi. Only one interface is active at a time (Circle supports one network interface).
  - **Pi 3 B/B+:** the USB Ethernet chip is probed before the network is set up, so the decision costs no reboot.
  - **Pi 4 B / CM4:** Circle's network layer owns the Ethernet driver, so the link can only be checked by starting the network on Ethernet. Without link, the firmware writes a marker file to the SD card and reboots once; the next boot uses Wi-Fi and deletes the marker. The following boot tries Ethernet again.
- **`wpa_supplicant.conf` only needed for Wi-Fi:** a board with Ethernet and no `wpa_supplicant.conf` always uses Ethernet and waits for a cable instead of entering the degraded state. Boards without Ethernet keep today's behavior.
- **CM4 without wireless becomes supported** (Ethernet only), and is removed from the list of unsupported boards.
- **Boot log** names the chosen interface and why (e.g. "Network: Ethernet, link up 1000 Mbit/s full duplex", "Network: Wi-Fi, no Ethernet link").
- **Selection logic** as a hardware-independent function in `src/core`, with host unit tests.
- **Documentation:** README (supported boards, Ethernet, first boot without `wpa_supplicant.conf`), CHANGELOG entry.

## Capabilities

### New Capabilities
<!-- none -->

### Modified Capabilities
- `connectivity`: the Wi-Fi bring-up requirement becomes a network bring-up requirement covering Ethernet and Wi-Fi; new requirement for interface selection at boot.
- `device-config`: `wpa_supplicant.conf` is only required when Wi-Fi is used.
- `board-support`: supported-boards table gains an Ethernet column and the CM4 without wireless; that board is removed from the unsupported list.

## Non-goals

- Switching interfaces at runtime (cable plugged in or pulled after boot); a reboot is needed.
- Wi-Fi and Ethernet at the same time, or failover between them.
- A configuration key to force an interface.
- USB Ethernet adapters on boards without a port (Zero W, Zero 2 W, 3 A+), even if Circle happens to have a driver for their chip.
- Static IP configuration, IPv6, PoE HAT support.
- Pi 400, Pi 5 / CM5 (still unsupported).

## Impact

- `src/fw/kernel.{h,cpp}`: USB host controller on `RASPPI == 3`, link probe, marker file, reboot, interface choice before `CNetSubSystem` is created; Wi-Fi chip only initialized when Wi-Fi is used.
- New `src/core/netselect.{h,cpp}` + `tests/test_netselect.cpp`.
- No change to the build targets, images, `config.txt`, or the SD card layout (the marker file is created only at runtime).
- Boot time without a cable on an Ethernet board: Pi 3 B/B+ +5 s; Pi 4/CM4 one extra reboot (about +15 s).
- `README.md`, `CHANGELOG.md`, `CLAUDE.md`; hardware tests need a Pi 3 B/B+ and a Pi 4 or CM4.
