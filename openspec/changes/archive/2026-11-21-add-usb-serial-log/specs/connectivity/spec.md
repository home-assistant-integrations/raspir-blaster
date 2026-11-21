## MODIFIED Requirements

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
