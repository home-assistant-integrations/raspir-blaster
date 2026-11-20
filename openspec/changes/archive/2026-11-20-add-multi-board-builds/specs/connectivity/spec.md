## MODIFIED Requirements

### Requirement: Wi-Fi and DHCP bring-up
At boot the firmware SHALL mount its boot storage (SD card, or eMMC on a CM4 with eMMC). It SHALL load the Wi-Fi firmware for the detected chip (CYW43430, 43436, 43455 or 43456) from `SD:/firmware/`, join the Wi-Fi network configured in `wpa_supplicant.conf`, and obtain an IPv4 address via DHCP using the host name `<device_id>`.

#### Scenario: Successful bring-up
- **WHEN** the device boots with valid Wi-Fi credentials and an access point in range
- **THEN** it obtains an IPv4 address via DHCP and logs the address

#### Scenario: Access point unavailable at boot
- **WHEN** the device boots while the access point is offline
- **THEN** it keeps retrying in the background and obtains an address once the access point is available, without a reboot

### Requirement: Hardware watchdog
The firmware SHALL enable the SoC hardware watchdog (BCM2835/BCM2837/BCM2711 power management watchdog) with a 10 s timeout and feed it only from the main loop, so that a hang in the main loop or any driver reboots the device.

#### Scenario: Main loop hangs
- **WHEN** the main loop stops running for more than 10 s
- **THEN** the device reboots and comes back online without manual intervention

#### Scenario: Normal operation
- **WHEN** the device runs normally, including during the longest permitted IR transmission
- **THEN** the watchdog never fires
