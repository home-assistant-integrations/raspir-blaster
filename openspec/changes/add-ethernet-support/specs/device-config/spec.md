## MODIFIED Requirements

### Requirement: Wi-Fi credentials file
The firmware SHALL read Wi-Fi credentials from `wpa_supplicant.conf` in the SD card root, in the format Circle's WPA supplicant expects. The file SHALL only be required on boards without Ethernet. On boards with Ethernet, a missing file means the firmware uses Ethernet (see connectivity, "Network interface selection").

#### Scenario: Wi-Fi file missing on a board without Ethernet
- **WHEN** `wpa_supplicant.conf` is missing on a Zero W, Zero 2 W or Pi 3 A+
- **THEN** the firmware logs an error, does not reboot, and blinks the ACT LED in the error pattern

#### Scenario: Wi-Fi file missing on a board with Ethernet
- **WHEN** `wpa_supplicant.conf` is missing on a Pi 3 B, Pi 3 B+, Pi 4 B or CM4
- **THEN** the firmware logs that it uses Ethernet because no Wi-Fi configuration exists, and does not enter the degraded state
