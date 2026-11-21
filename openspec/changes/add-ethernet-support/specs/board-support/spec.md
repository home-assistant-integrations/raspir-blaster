## MODIFIED Requirements

### Requirement: Supported boards and images
The firmware SHALL support the following boards, each through the listed build target and kernel image, with the listed network interfaces:

| Board                          | SoC      | Build target | Mode    | Image              | Boot media | Network          |
| ------------------------------ | -------- | ------------ | ------- | ------------------ | ---------- | ---------------- |
| Raspberry Pi Zero W            | BCM2835  | `rpi0w`      | AArch32 | `kernel.img`       | SD card    | Wi-Fi            |
| Raspberry Pi Zero 2 W          | BCM2710A1| `rpi3`       | AArch64 | `kernel8.img`      | SD card    | Wi-Fi            |
| Raspberry Pi 3 A+              | BCM2837  | `rpi3`       | AArch64 | `kernel8.img`      | SD card    | Wi-Fi            |
| Raspberry Pi 3 B / 3 B+        | BCM2837  | `rpi3`       | AArch64 | `kernel8.img`      | SD card    | Ethernet, Wi-Fi  |
| Raspberry Pi 4 B               | BCM2711  | `rpi4`       | AArch64 | `kernel8-rpi4.img` | SD card    | Ethernet, Wi-Fi  |
| Compute Module 4 Lite          | BCM2711  | `rpi4`       | AArch64 | `kernel8-rpi4.img` | SD card    | Ethernet, Wi-Fi  |
| Compute Module 4 with eMMC     | BCM2711  | `rpi4-emmc`  | AArch64 | `kernel8-rpi4.img` | eMMC       | Ethernet, Wi-Fi  |
| Compute Module 4 without wireless (Lite or eMMC) | BCM2711 | `rpi4` / `rpi4-emmc` | AArch64 | `kernel8-rpi4.img` | SD card / eMMC | Ethernet |

On a CM4, Ethernet needs a carrier board with an Ethernet jack.

#### Scenario: Each target builds its image
- **WHEN** all targets are built
- **THEN** one kernel image exists per target, built with the Circle configuration of that target

#### Scenario: Same firmware features on every board
- **WHEN** the firmware runs on any supported board
- **THEN** IR receive, IR transmit, MQTT, Home Assistant discovery, configuration and watchdog behave as specified in the other capabilities, on every network interface the board supports

### Requirement: Unsupported boards
Boards outside the supported list SHALL NOT be listed in the generated `config.txt`. The documentation SHALL name them as unsupported: Pi 5, CM5, Pi 400, Pi 1, Pi 2, Zero without W, and CM3/CM3+.

#### Scenario: Pi 5
- **WHEN** the universal card is inserted into a Pi 5
- **THEN** no raspir-blaster kernel is started (the Pi 5 kernel image `kernel_2712.img` does not exist on the card)

#### Scenario: CM4 without wireless and without Ethernet link
- **WHEN** the firmware runs on a CM4 without a Wi-Fi chip, with `wpa_supplicant.conf` present and no Ethernet link
- **THEN** it reboots once to try Wi-Fi, logs "Cannot initialize Wi-Fi", and enters the fatal state, so the watchdog reboots it and the next boot tries Ethernet again
