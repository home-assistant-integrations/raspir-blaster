## ADDED Requirements

### Requirement: Recorded test runs
Every hardware verification run SHALL be recorded in `docs/dev/hardware-test-report.md`. Each record SHALL contain the date, the firmware version and git commit, the Circle tag and toolchain version, the hardware setup (receiver, transmitter, driver stage, measurement equipment), and for every test in this specification a result of `pass`, `fail` or `not run`, with measured values and notes.

#### Scenario: Complete record
- **WHEN** a verification run is finished
- **THEN** the report contains one entry with all fields above, and every test of this specification is listed with a result

#### Scenario: Failed test
- **WHEN** a test fails
- **THEN** the report entry contains the observed behavior and a reference to the follow-up change or issue that addresses it

### Requirement: Boot smoke test
The firmware image from `make sdcard` SHALL boot on the Raspberry Pi Zero W. The serial console (UART0, 115200 baud) SHALL show the firmware banner with the version, the mounted SD card, the effective configuration with the password masked, the IP address and the MQTT connection result.

#### Scenario: Normal boot
- **WHEN** the device boots from a card prepared with `make sdcard` and valid `raspir-blaster.conf` and `wpa_supplicant.conf` files
- **THEN** within 30 s the serial log shows the banner, "SD card mounted", the config line with `mqtt_password=***` (if a password is set), "Network up, IP address …" and "Connected to broker"

### Requirement: Watchdog recovery
A firmware hang SHALL be recovered by the hardware watchdog without manual intervention.

#### Scenario: Forced hang
- **WHEN** an image built with `make WATCHDOG_TEST=1` is booted
- **THEN** the log shows "Watchdog test: hanging main loop" about 30 s after boot, and the device reboots 10 s (±2 s) later and shows the boot banner again

#### Scenario: No spurious reboot
- **WHEN** the normal image runs for 30 minutes, including at least ten transmissions of a 2 s frame
- **THEN** no reboot occurs

### Requirement: Home Assistant integration and availability
The device SHALL appear in Home Assistant through MQTT Discovery and SHALL report its availability correctly through broker outages and power loss.

#### Scenario: Discovery
- **WHEN** the device connects to the broker used by Home Assistant
- **THEN** Home Assistant shows the device "raspir-blaster (`<device_id>`)" with the sensor "IR Receiver", the model `Raspberry Pi Zero W (BCM2835, Circle bare-metal)` and the firmware version, and the sensor is available

#### Scenario: Broker restart
- **WHEN** the broker is stopped for 2 minutes and then started again
- **THEN** the serial log shows the disconnect and retries spaced 5, 10, 20, 40, 60 s apart. After the broker is back, the device reconnects at the next retry, and Home Assistant shows the sensor as available again

#### Scenario: Power loss
- **WHEN** the device loses power while connected
- **THEN** Home Assistant shows the sensor as unavailable within 90 s (1.5 × the 60 s keep-alive)

### Requirement: Capture of real remotes
Frames from real remote controls SHALL be captured and published with timings that match the protocol.

#### Scenario: NEC remote
- **WHEN** a button of an NEC remote is pressed once
- **THEN** a frame is published that starts with a mark of 9000 µs ±10 % and a space of 4500 µs ±10 %, contains 67 timings, and has bit marks of 560 µs ±20 %. NEC repeat codes (3 timings) fall below the 4-timing noise threshold of `ir-receive` and are not published.

#### Scenario: RC5 remote
- **WHEN** a button of an RC5 remote is pressed once
- **THEN** a frame is published whose timings are multiples of about 889 µs (each within ±20 % of 889 or 1778 µs)

#### Scenario: Air-conditioner remote
- **WHEN** a button of an air-conditioner remote is pressed
- **THEN** the complete frame is published as one message (after raising `ir_frame_gap_ms` if the default 15 ms splits it), and the value used for `ir_frame_gap_ms` is recorded in the report

### Requirement: Carrier and timing accuracy
The transmitted signal on GPIO18 SHALL meet the carrier and timing requirements of `ir-transmit`, measured with a logic analyzer or oscilloscope.

#### Scenario: Carrier frequency and duty cycle
- **WHEN** frames with `freq` 36000, 38000 and 40000 are transmitted
- **THEN** the measured carrier frequency during marks is within ±1 % of the requested value, and the duty cycle is 33 % ± 3 %

#### Scenario: Idle level
- **WHEN** no transmission is in progress, including directly after boot
- **THEN** GPIO18 is low

#### Scenario: Edge timing of a long frame
- **WHEN** a frame of 300 timings with a total duration of about 400 ms is transmitted
- **THEN** every mark starts and ends within ±50 µs of its nominal time measured from the first mark, including the last edge

### Requirement: End-to-end replay
A code captured by the device SHALL control the original device when replayed from Home Assistant, and the device SHALL NOT capture its own transmission.

#### Scenario: TV power
- **WHEN** the TV power code is captured, its `raw` attribute is copied into a Home Assistant MQTT button, and the button is pressed
- **THEN** the TV reacts as with the original remote

#### Scenario: No self-capture
- **WHEN** the device transmits while its receiver faces the IR LED
- **THEN** no message is published to `<topic_base>/received` as a result of the transmission

### Requirement: Ethernet selection and operation
On boards with Ethernet, the firmware SHALL choose the network interface at boot as specified in connectivity ("Network interface selection"), and SHALL work over Ethernet like over Wi-Fi. These tests run on a Pi 3 B or B+ and on a Pi 4 B or CM4 (on a carrier board with an Ethernet jack), connected to a switch on the LAN with a DHCP server and the MQTT broker.

#### Scenario: Pi 4 with cable
- **WHEN** a Pi 4 B (or CM4) with `wpa_supplicant.conf` boots with an Ethernet cable connected to an active switch
- **THEN** the serial log shows "Network: Ethernet (link up, …)" with the link speed and no Wi-Fi initialization, "Network up, IP address …" and "Connected to broker" within 30 s, and IR send and receive work as in the other tests

#### Scenario: Pi 4 without cable
- **WHEN** a Pi 4 B (or CM4) with `wpa_supplicant.conf` boots without an Ethernet cable
- **THEN** the log shows "No Ethernet link, rebooting to use Wi-Fi" after at most 5 s of link checking, the device reboots once, the second boot logs "Network: Wi-Fi (after Ethernet check)" and connects to the broker, and `raspir-blaster.wifi` is no longer on the card; after a power cycle with the cable connected it uses Ethernet again

#### Scenario: Pi 3 with and without cable
- **WHEN** a Pi 3 B or B+ with `wpa_supplicant.conf` boots once with and once without an Ethernet cable
- **THEN** with cable it logs "Network: Ethernet (link up, …)"; without cable it logs "Network: Wi-Fi (no Ethernet link)" at most 5 s after USB enumeration, without a reboot; in both cases it connects to the broker

#### Scenario: No Wi-Fi configuration
- **WHEN** a board with Ethernet without `wpa_supplicant.conf` boots without a cable, and the cable is connected 2 minutes later
- **THEN** it logs "Network: Ethernet (no SD:/wpa_supplicant.conf)", does not enter the degraded state and does not reboot, and after the cable is connected it obtains an IP address and connects to the broker

#### Scenario: Cable pulled at runtime
- **WHEN** the device runs on Ethernet and the cable is pulled for 2 minutes and plugged in again
- **THEN** the device stays on Ethernet without a reboot, the log shows the MQTT disconnect and retries with backoff, and after the cable is back it reconnects and Home Assistant shows the sensor as available again

### Requirement: Log over USB
On the boards with USB device mode, the log SHALL be readable over a single USB cable as specified in connectivity ("Diagnostic log on serial"). These tests run on a Zero W and on a Pi 4 B, each powered only by a USB cable to a Mac, with a terminal (`screen /dev/cu.usbmodem* 115200`); the Pi 3 B check uses any Pi 3 B or B+.

#### Scenario: Boot log replay
- **WHEN** the board boots connected to the Mac and the terminal is opened only after the device has connected to MQTT
- **THEN** within 10 s of boot `/dev/cu.usbmodem*` exists, and the terminal shows the log from the firmware banner on with CR LF line endings, followed by live lines (e.g. after an IR frame is received), while the UART log is unchanged

#### Scenario: Reopen
- **WHEN** the terminal is closed and opened again
- **THEN** the kept log is shown again before the live output

#### Scenario: No terminal
- **WHEN** the board runs for 10 minutes with the cable connected but no terminal open, while IR frames are sent and received
- **THEN** IR, MQTT and the watchdog behave normally (no reboot), and opening the terminal afterwards shows the most recent log

#### Scenario: USB identity
- **WHEN** the board is connected
- **THEN** the Mac's System Information lists the USB device "raspir-blaster (<device_id>)" with vendor ID `0x1209`

#### Scenario: Pi 3 B without USB device
- **WHEN** a Pi 3 B or B+ is running
- **THEN** no USB serial device appears on a connected computer, the UART log shows no "Cannot start USB serial log" warning, and Ethernet works as in "Ethernet selection and operation"
