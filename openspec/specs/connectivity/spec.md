# connectivity Specification

## Purpose
How the device joins Wi-Fi and stays connected to the MQTT broker: connection, reconnect backoff, availability (LWT) and the watchdog-backed error states.
## Requirements
### Requirement: Wi-Fi and DHCP bring-up
At boot the firmware SHALL mount its boot storage (SD card, or eMMC on a CM4 with eMMC). It SHALL load the Wi-Fi firmware for the detected chip (CYW43430, 43436, 43455 or 43456) from `SD:/firmware/`, join the Wi-Fi network configured in `wpa_supplicant.conf`, and obtain an IPv4 address via DHCP using the host name `<device_id>`.

#### Scenario: Successful bring-up
- **WHEN** the device boots with valid Wi-Fi credentials and an access point in range
- **THEN** it obtains an IPv4 address via DHCP and logs the address

#### Scenario: Access point unavailable at boot
- **WHEN** the device boots while the access point is offline
- **THEN** it keeps retrying in the background and obtains an address once the access point is available, without a reboot

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

#### Scenario: Boot log
- **WHEN** the device boots with a serial adapter attached
- **THEN** the log shows each initialization step, the IP address and the MQTT connection result

