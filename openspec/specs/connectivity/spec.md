# connectivity Specification

## Purpose
How the device connects to the network (Ethernet or Wi-Fi, chosen at boot) and stays connected to the MQTT broker: connection, reconnect backoff, availability (LWT) and the watchdog-backed error states.
## Requirements
### Requirement: MQTT connection
Once the network is up, the firmware SHALL connect to the broker given by `mqtt_host`/`mqtt_port` using MQTT 3.1.1. It SHALL use client identifier `<device_id>`, the configured credentials (if any) and a keep-alive of 60 s. It SHALL register a Last Will on `<topic_base>/status` with payload `offline`, retained.

#### Scenario: Connect with credentials
- **WHEN** `mqtt_user` and `mqtt_password` are configured and the broker accepts them
- **THEN** the firmware connects using those credentials and the Last Will described above

#### Scenario: Broker rejects credentials
- **WHEN** the broker refuses the connection
- **THEN** the firmware logs the reason and retries according to the reconnect policy

### Requirement: Actions after connecting
After every successful MQTT connection, the firmware SHALL publish `online` retained to `<topic_base>/status`, subscribe to `<topic_base>/send`, and publish the Home Assistant discovery configuration.

#### Scenario: Connection established
- **WHEN** the broker acknowledges the connection
- **THEN** `<topic_base>/status` holds the retained payload `online`, the device is subscribed to `<topic_base>/send`, and the discovery configuration is published

### Requirement: Automatic reconnect with backoff
When the MQTT connection is lost or cannot be established while the network is up, the firmware SHALL retry. The first retry happens after 5 s, and the interval doubles after each failed attempt up to a maximum of 60 s. A successful connection resets the interval to 5 s. While the network is down, no MQTT attempts SHALL be made.

#### Scenario: Broker restart
- **WHEN** the broker restarts while the device is connected
- **THEN** the device detects the disconnect, reconnects within 5 s of the broker becoming reachable again (first retry), and publishes `online` and the discovery configuration again

#### Scenario: Prolonged outage
- **WHEN** the broker stays unreachable for 10 minutes
- **THEN** connection attempts are spaced 5, 10, 20, 40, 60, 60, … seconds apart

#### Scenario: Wi-Fi loss
- **WHEN** the Wi-Fi connection drops
- **THEN** the MQTT state is treated as disconnected, no MQTT attempts are made until the network is up again, and the broker publishes the Last Will `offline`

### Requirement: Hardware watchdog
The firmware SHALL enable the SoC hardware watchdog (BCM2835/BCM2837/BCM2711 power management watchdog) with a 10 s timeout and feed it only from the main loop, so that a hang in the main loop or any driver reboots the device.

#### Scenario: Main loop hangs
- **WHEN** the main loop stops running for more than 10 s
- **THEN** the device reboots and comes back online without manual intervention

#### Scenario: Normal operation
- **WHEN** the device runs normally, including during the longest permitted IR transmission
- **THEN** the watchdog never fires

### Requirement: Diagnostic log on serial
The firmware SHALL write log messages (boot steps, network state changes, MQTT state changes, rejected messages, errors) to the serial console on UART0 (GPIO14/GPIO15) at 115200 baud.

On boards whose USB controller supports device mode, the firmware SHALL additionally present a USB serial device (CDC ACM) and write the same log output to it:

| Board | USB port to the host computer |
| --- | --- |
| Zero W, Zero 2 W | micro-USB port marked "USB" |
| Pi 3 A+ | USB-A port |
| Pi 4 B | USB-C power port |

The Pi 3 B, Pi 3 B+ and CM4 SHALL NOT present the USB serial device; their USB setup is unchanged.

For the USB serial device:
- The firmware SHALL keep the most recent 16 KB of log output in RAM. Each time the host sets DTR (a terminal opens the port), the firmware SHALL first send the kept output, oldest first, and then the live output.
- Writing the log SHALL NOT block: while DTR is not set or the host does not read, USB output is not sent and the oldest kept output is overwritten. If kept output was overwritten while a terminal was open, the firmware SHALL send a marker line naming the number of lost bytes.
- The device SHALL identify itself with the vendor ID `0x1209` (pid.codes), the manufacturer string "raspir-blaster" and the product string "raspir-blaster (<device_id>)".
- Line endings SHALL be sent as CR LF.

#### Scenario: Boot log
- **WHEN** the device boots with a serial adapter attached
- **THEN** the log shows each initialization step, the IP address and the MQTT connection result

#### Scenario: Log over USB on a Zero W
- **WHEN** a Zero W is powered from a Mac through its "USB" port and `screen /dev/cu.usbmodem* 115200` is started after the device has connected to MQTT
- **THEN** the terminal first shows the boot log from the firmware banner on, then new log lines as they occur, and the UART0 log is unchanged

#### Scenario: Terminal reopened
- **WHEN** the terminal is closed and opened again
- **THEN** the kept log (up to 16 KB) is shown again before the live output

#### Scenario: No terminal
- **WHEN** the USB cable is connected to a computer but no terminal opens the port for an hour
- **THEN** IR, MQTT and the watchdog work as without USB, and opening a terminal afterwards shows the most recent 16 KB of log

#### Scenario: Board without USB device mode
- **WHEN** the firmware runs on a Pi 3 B, Pi 3 B+ or CM4
- **THEN** no USB serial device is presented, the log goes to UART0 only, and Ethernet on the Pi 3 B/B+ works as before

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

