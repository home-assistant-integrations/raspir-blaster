## RENAMED Requirements

- FROM: `### Requirement: Wi-Fi and DHCP bring-up`
- TO: `### Requirement: Network and DHCP bring-up`

## MODIFIED Requirements

### Requirement: Network and DHCP bring-up
At boot the firmware SHALL mount its boot storage (SD card, or eMMC on a CM4 with eMMC) and bring up exactly one network interface, chosen as described in "Network interface selection":
- **Ethernet:** the board's Ethernet controller (USB LAN9514 on the Pi 3 B, USB LAN7515 on the Pi 3 B+, GENET on the Pi 4 B and CM4).
- **Wi-Fi:** the firmware SHALL load the Wi-Fi firmware for the detected chip (CYW43430, 43436, 43455 or 43456) from `SD:/firmware/` and join the Wi-Fi network configured in `wpa_supplicant.conf`. The Wi-Fi chip SHALL NOT be initialized when Ethernet is used.

On either interface the firmware SHALL obtain an IPv4 address via DHCP using the host name `<device_id>`.

#### Scenario: Successful bring-up
- **WHEN** the device boots with valid Wi-Fi credentials and an access point in range, or with an Ethernet cable connected to a network with a DHCP server
- **THEN** it obtains an IPv4 address via DHCP and logs the address

#### Scenario: Access point unavailable at boot
- **WHEN** the device boots on Wi-Fi while the access point is offline
- **THEN** it keeps retrying in the background and obtains an address once the access point is available, without a reboot

#### Scenario: DHCP server unavailable on Ethernet
- **WHEN** the device uses Ethernet and no DHCP server answers at boot
- **THEN** it keeps retrying in the background and obtains an address once a DHCP server answers, without a reboot

## ADDED Requirements

### Requirement: Network interface selection
The firmware SHALL choose the network interface at boot, once, from the board model, the presence of `wpa_supplicant.conf`, and the Ethernet link state. Boards with Ethernet are the Pi 3 B, Pi 3 B+, Pi 4 B and CM4; the other supported boards have none.

| Board has Ethernet | `wpa_supplicant.conf` | Ethernet link within 5 s | Interface |
| --- | --- | --- | --- |
| no | present | — | Wi-Fi |
| no | missing | — | none: degraded state (see device-config) |
| yes | missing | not checked | Ethernet (waits for a cable) |
| yes | present | up | Ethernet |
| yes | present | down | Wi-Fi |

The link check SHALL take at most 5 s from the start of the check.
- **Pi 3 B/B+:** the firmware SHALL check the link of the USB Ethernet chip before the network is set up, and then set up the chosen interface without a reboot.
- **Pi 4 B / CM4:** the firmware SHALL set up the network on Ethernet and check the link. Without link, it SHALL create the marker file `SD:/raspir-blaster.wifi` and reboot. At the next boot, if the marker file exists, the firmware SHALL delete it and use Wi-Fi without checking the link. The marker file SHALL only be created when `wpa_supplicant.conf` is present.

The interface SHALL NOT change at runtime: a cable plugged in or pulled after boot takes effect at the next boot. The boot log SHALL name the chosen interface and the reason, and on Ethernet the link speed if known.

#### Scenario: Pi 4 with cable
- **WHEN** a Pi 4 B with `wpa_supplicant.conf` boots with an Ethernet cable connected to an active switch
- **THEN** it uses Ethernet, does not initialize the Wi-Fi chip, and logs "Ethernet" with the link speed

#### Scenario: Pi 4 without cable
- **WHEN** a Pi 4 B with `wpa_supplicant.conf` boots without an Ethernet cable
- **THEN** it logs that there is no Ethernet link, creates `raspir-blaster.wifi`, and reboots; after the reboot it deletes `raspir-blaster.wifi`, uses Wi-Fi, and connects to MQTT

#### Scenario: Next power-up tries Ethernet again
- **WHEN** a Pi 4 B that is running on Wi-Fi after the marker reboot is power-cycled with a cable connected
- **THEN** it uses Ethernet

#### Scenario: Pi 3 B+ without cable
- **WHEN** a Pi 3 B+ with `wpa_supplicant.conf` boots without an Ethernet cable
- **THEN** it uses Wi-Fi after at most 5 s of link checking, without a reboot

#### Scenario: Ethernet board without Wi-Fi configuration
- **WHEN** a Pi 4 B or Pi 3 B without `wpa_supplicant.conf` boots without a cable
- **THEN** it uses Ethernet, does not enter the degraded state, does not reboot, and obtains an address once a cable is connected

#### Scenario: CM4 without Wi-Fi chip
- **WHEN** a CM4 without wireless and without `wpa_supplicant.conf` boots on a carrier board with a connected Ethernet jack
- **THEN** it uses Ethernet and connects to MQTT

#### Scenario: Board without Ethernet
- **WHEN** a Zero W, Zero 2 W or Pi 3 A+ boots
- **THEN** it uses Wi-Fi without any Ethernet check, exactly as before this change

#### Scenario: Cable pulled at runtime
- **WHEN** the device runs on Ethernet and the cable is pulled
- **THEN** it stays on Ethernet, the MQTT connection is retried with backoff, and it reconnects when the cable is plugged in again
