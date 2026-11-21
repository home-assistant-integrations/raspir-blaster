## MODIFIED Requirements

### Requirement: Network and DHCP bring-up
At boot the firmware SHALL mount its boot storage (SD card, or eMMC on a CM4 with eMMC) and bring up exactly one network interface, chosen as described in "Network interface selection":
- **Ethernet:** the board's Ethernet controller (USB LAN9514 on the Pi 3 B, USB LAN7515 on the Pi 3 B+, GENET on the Pi 4 B and CM4).
- **Wi-Fi:** the firmware SHALL load the Wi-Fi firmware for the detected chip (CYW43430, 43436, 43455 or 43456) from `SD:/firmware/` and join the Wi-Fi network configured in `wpa_supplicant.conf`. The Wi-Fi chip SHALL NOT be initialized when Ethernet is used.

On either interface the firmware SHALL obtain an IPv4 address via DHCP using `device_id` as the host name, unchanged (see device-config: `device_id`'s allowed characters are restricted to `[a-z0-9-]`, so it is already a valid host name). The MQTT client ID and topics also use `device_id` unchanged.

#### Scenario: Successful bring-up
- **WHEN** the device boots with valid Wi-Fi credentials and an access point in range, or with an Ethernet cable connected to a network with a DHCP server
- **THEN** it obtains an IPv4 address via DHCP and logs the address

#### Scenario: Access point unavailable at boot
- **WHEN** the device boots on Wi-Fi while the access point is offline
- **THEN** it keeps retrying in the background and obtains an address once the access point is available, without a reboot

#### Scenario: DHCP server unavailable on Ethernet
- **WHEN** the device uses Ethernet and no DHCP server answers at boot
- **THEN** it keeps retrying in the background and obtains an address once a DHCP server answers, without a reboot

#### Scenario: Host name from the default device_id
- **WHEN** `device_id` is the default `pi-zero-ir`
- **THEN** the DHCP host name and the MQTT client ID are both `pi-zero-ir`

## ADDED Requirements

### Requirement: Local network announcement
Once the network is up (normal mode) or the access point is open (setup mode), the firmware SHALL announce itself with mDNS (Bonjour):
- it SHALL answer queries for `<hostname>.local` with its IPv4 address; if another host already uses that name, it SHALL use `<hostname>-2.local` (and so on) and log the name it uses,
- it SHALL publish a DNS-SD service of type `_http._tcp` on port 80 named `raspir-blaster (<device_id>)`, with TXT records `version=<firmware version>`, `board=<board name>`, `mode=normal` or `mode=setup`, and `api=/api/`.

#### Scenario: Open by name
- **WHEN** the device `pi-zero-ir` is running and a Mac on the same network requests `http://pi-zero-ir.local/api/status`
- **THEN** the status is returned

#### Scenario: Service browsing
- **WHEN** `dns-sd -B _http._tcp` runs on a Mac on the same network
- **THEN** the list contains "raspir-blaster (pi-zero-ir)", and `dns-sd -L` on it shows port 80 and the version, board, mode and api TXT records

#### Scenario: Name conflict
- **WHEN** two devices with the same `device_id` are on the network
- **THEN** the second one answers to `<hostname>-2.local` and logs that name

#### Scenario: Setup mode
- **WHEN** a phone has joined the setup access point
- **THEN** `<hostname>.local` resolves to 192.168.4.1 and the service has `mode=setup`
