# `raspir-blaster`

[![CI](https://github.com/home-assistant-integrations/raspir-blaster/actions/workflows/ci.yml/badge.svg)](https://github.com/home-assistant-integrations/raspir-blaster/actions/workflows/ci.yml)

`raspir-blaster` is a Raspberry Pi based IR blaster that integrates
with [Home Assistant](https://www.home-assistant.io). It runs as bare-metal
firmware built on [Circle](https://github.com/rsta2/circle), with no Linux,
and talks to Home Assistant over MQTT:

- IR signals from any remote are captured as raw mark/space timings and
  published to MQTT.
- Raw timings published to MQTT are sent out through an IR LED.

The firmware does not decode IR protocols. It records and replays raw timings,
so NEC, Sony, RC5/RC6 and long air-conditioner frames all work the same way.

Ready-to-use SD card files are attached to every
[release](https://github.com/home-assistant-integrations/raspir-blaster/releases);
see [Releases](#releases). To build the firmware yourself, see [Building](#building).

## Supported boards

| Board                      | Image              | Boot media | Status                     |
| -------------------------- | ------------------ | ---------- | -------------------------- |
| Raspberry Pi Zero W        | `kernel.img`       | SD card    | built, not yet verified    |
| Raspberry Pi Zero 2 W      | `kernel8.img`      | SD card    | built, not yet verified    |
| Raspberry Pi 3 B / B+ / A+ | `kernel8.img`      | SD card    | built, not yet verified    |
| Raspberry Pi 4 B           | `kernel8-rpi4.img` | SD card    | built, not yet verified    |
| Compute Module 4 Lite      | `kernel8-rpi4.img` | SD card    | built, not yet verified    |
| Compute Module 4 with eMMC | `kernel8-rpi4.img` | eMMC       | built, not yet verified    |

"Verified" means the hardware tests of the OpenSpec change
`verify-ir-blaster-hardware` passed on that board.

Not supported: Raspberry Pi 5, Compute Module 5, Pi 400, Pi 1, Pi 2, Pi Zero
(without W), CM3/CM3+, and CM4 modules without wireless. The firmware needs
on-board Wi-Fi, and the Pi 5's RP1 I/O chip needs different drivers.

## Hardware

- One of the supported boards above
- IR receiver TSOP38238 (38 kHz demodulator)
- IR transmitter module AZ-Delivery KY-005

The wiring is the same on every board (on a CM4, use the 40-pin header of the
carrier board):

| Module    | Module pin | Pi header pin | Signal                           |
| --------- | ---------- | ------------- | -------------------------------- |
| TSOP38238 | Signal     | 16            | GPIO23 (edge interrupt)          |
| TSOP38238 | GND        | 9             | GND                              |
| TSOP38238 | VCC        | 1             | 3.3 V                            |
| KY-005    | Signal     | 12            | GPIO18 (hardware PWM0, carrier)  |
| KY-005    | GND        | 6             | GND                              |
| KY-005    | VCC        | 2             | 5 V                              |

Header pin numbers and GPIO numbers are different things: the transmitter
signal goes to **header pin 12**, which is **GPIO18**.

Driving the IR LED directly from GPIO18 limits the range to a few metres. For
more range, drive the LED through an NPN transistor (e.g. 2N2222, 1 kΩ base
resistor, 100 Ω LED resistor at 5 V). The firmware stays the same.

A serial adapter on UART0 (header pins 8 = TX, 10 = RX, 115200 baud) shows the
firmware log, including the detected board.

## Building

Requirements: macOS on Apple Silicon, `make`, `git`, `curl`, and Xcode's
command line tools (for `clang++`, used by the host tests).

```sh
make                       # first run: downloads toolchains + Circle into toolchain/,
                           # then builds the images of all targets
make TARGETS="rpi0w rpi4"  # builds only these targets (faster during development)
make VERSION=2026.11.0     # sets the firmware version (default: VERSION file, else "dev")
make sdcard                # assembles build/sdcard/ and build/cm4-emmc/
make test                  # runs the host unit tests for the hardware-independent code
make clean                 # removes build outputs, keeps toolchain/
make distclean             # also removes toolchain/
```

| Target      | Boards                     | Mode   | Output                                      |
| ----------- | -------------------------- | ------ | ------------------------------------------- |
| `rpi0w`     | Zero W                     | 32 bit | `build/images/rpi0w/kernel.img`             |
| `rpi3`      | Zero 2 W, Pi 3             | 64 bit | `build/images/rpi3/kernel8.img`             |
| `rpi4`      | Pi 4 B, CM4 Lite           | 64 bit | `build/images/rpi4/kernel8-rpi4.img`        |
| `rpi4-emmc` | CM4 with eMMC              | 64 bit | `build/images/rpi4-emmc/kernel8-rpi4.img`   |

Pinned versions are in [scripts/versions.env](scripts/versions.env): the ARM
GNU Toolchains (`arm-none-eabi`, `aarch64-none-elf`), the Circle release, and
the commits of the Raspberry Pi
[firmware](https://github.com/raspberrypi/firmware) and
[Wi-Fi firmware](https://github.com/RPi-Distro/firmware-nonfree)
repositories. Every downloaded file is checked against a SHA-256 checksum;
boot and Wi-Fi files are listed in
[scripts/firmware-files.txt](scripts/firmware-files.txt). After changing a
pinned firmware commit, run `make firmware-checksums` and review the result.

The first build compiles Circle once per target and takes a few minutes;
later builds only recompile the firmware.

## SD card setup

`make sdcard` needs the images of all targets (run `make` without `TARGETS`).
`build/sdcard/` boots every supported board except a CM4 with eMMC: its
`config.txt` selects the right image and CPU mode for each board.

1. Format a micro SD card with a single FAT32 partition.
2. Run `make sdcard` and copy the **contents** of `build/sdcard/` to the card.
3. On the card, rename `wpa_supplicant.conf.example` to `wpa_supplicant.conf`
   and enter your Wi-Fi SSID and password.
4. On the card, rename `raspir-blaster.conf.example` to `raspir-blaster.conf`
   and set at least `mqtt_host`.
5. Insert the card and power the Pi.

### Compute Module 4 with eMMC

A CM4 with eMMC boots from its eMMC, so it uses `build/cm4-emmc/` instead:

1. Set the carrier board's boot jumper to USB boot (e.g. "Fit jumper to
   disable eMMC Boot" on the CM4 IO Board), connect the USB port to the Mac
   and run [`rpiboot`](https://github.com/raspberrypi/usbboot). The eMMC then
   appears as a USB drive.
2. Make sure the eMMC has a FAT32 partition, then copy the **contents** of
   `build/cm4-emmc/` to it.
3. Rename and edit the two example configuration files as above.
4. Remove the jumper and power-cycle the module.

If the configuration is missing or invalid, the ACT LED blinks slowly
(1 s on, 1 s off) and the serial log tells you what is wrong. A fast blink
means a hardware or driver error; the device then reboots after 10 s.

## Releases

Releases are published on the
[Releases page](https://github.com/home-assistant-integrations/raspir-blaster/releases).
Each release has:

| File | For |
| --- | --- |
| `raspir-blaster-<version>-sdcard.zip` | Zero W, Zero 2 W, Pi 3, Pi 4 B, CM4 Lite: extract onto a FAT32 SD card |
| `raspir-blaster-<version>-cm4-emmc.zip` | CM4 with eMMC: copy onto the eMMC (see above) |
| `SHA256SUMS.txt` | checksums; verify with `shasum -a 256 -c SHA256SUMS.txt` |

**Version scheme:** `yyyy.mm.rm`, with the year and two-digit month of the
release (UTC) and a release counter `rm` that starts at `0` every month, e.g.
`2026.11.0`, `2026.11.1`, `2026.12.0`. The firmware reports its version in the
serial boot log and as the firmware version of the Home Assistant device.
Builds outside a release report `dev`.

### Making a release

Maintainers start a release under **Actions → Release → Run workflow** on
`main`. The workflow:

1. computes the next version and creates the branch `rel-<version>` with a
   `VERSION` file,
2. builds and tests the firmware on that branch,
3. publishes the GitHub release `<version>` with the files above.

If a step fails, the workflow deletes the release branch again, so the version
number is used by the next run. Only one release runs at a time.

Repository settings this needs: GitHub Actions enabled, workflow permissions
allowing `contents: write`, and no branch protection or ruleset on `rel-*`
branches. If a cleanup ever fails, delete a stray branch with
`git push origin --delete rel-<version>`.

## Continuous integration

Every push to any branch except `rel-*` (including `main` after a pull request
is merged) runs the [CI workflow](.github/workflows/ci.yml) on an Apple
Silicon macOS runner: it builds all targets, fails on compiler warnings, runs
`make test` and `make sdcard`. The kernel images of each successful run can
be downloaded from the run's page for 7 days.

## Configuration (`raspir-blaster.conf`)

One `key=value` per line, `#` starts a comment.

| Key               | Required | Default           | Allowed values                              |
| ----------------- | -------- | ----------------- | ------------------------------------------- |
| `mqtt_host`       | yes      |                   | IPv4 address or host name, max 63 chars     |
| `mqtt_port`       | no       | `1883`            | 1–65535                                     |
| `mqtt_user`       | no       | anonymous         | max 63 chars                                |
| `mqtt_password`   | no       |                   | max 63 chars, only used with `mqtt_user`    |
| `device_id`       | no       | `pi_zero_ir`      | 1–31 chars of `a-z`, `0-9`, `_`             |
| `topic_base`      | no       | `home/ir_blaster` | 1–63 chars, no `+`, `#` or trailing `/`     |
| `ir_frame_gap_ms` | no       | `15`              | 5–60                                        |

`device_id` is the MQTT client ID, the DHCP host name and the Home Assistant
device identifier. Give every blaster on the same broker its own `device_id`
and `topic_base`.

`ir_frame_gap_ms` is the silence that ends a received frame. Increase it if
the remote of an air conditioner sends its frame in several parts and they
show up as separate frames.

## MQTT interface

| Topic                                               | Direction   | Payload                                     |
| --------------------------------------------------- | ----------- | ------------------------------------------- |
| `<topic_base>/send`                                 | HA → device | IR frame to transmit                        |
| `<topic_base>/received`                             | device → HA | IR frame captured by the receiver           |
| `<topic_base>/status`                               | device → HA | `online` / `offline` (retained)             |
| `homeassistant/sensor/<device_id>/received/config` | device → HA | MQTT Discovery config (retained)            |

An IR frame is a JSON object:

```json
{"freq": 38000, "raw": [9000, 4500, 560, 560, 560, 1690]}
```

- `raw` (required): alternating mark and space durations in microseconds,
  starting with a mark. 1–512 values, each 1–65535, at most 2 s in total.
- `freq` (optional, default 38000): carrier frequency in Hz, 20000–60000.

Invalid messages are rejected and logged; nothing is transmitted. Retained
messages on `<topic_base>/send` are ignored, so an old command is never
replayed after a reconnect. Received frames always report `freq` 38000,
because the receiver cannot measure the carrier.

## Home Assistant

The device appears automatically under **Settings → Devices & services →
MQTT** as "raspir-blaster (`<device_id>`)", with an "IR Receiver" sensor. The
sensor's state is the number of timings of the last received frame; its
attributes hold `freq` and `raw`.

### Learning a remote button

1. Point the remote at the receiver and press the button.
2. In Home Assistant, open **Developer Tools → States** and select the
   "IR Receiver" sensor.
3. Copy the `raw` attribute.
4. Create a button (or script) that publishes it:

```yaml
mqtt:
  button:
    - name: "TV Power"
      command_topic: "home/ir_blaster/send"
      payload_press: '{"freq": 38000, "raw": [9000, 4500, 560, 560, 560, 1690]}'
```

For 36 kHz (RC5/RC6) or 40 kHz (Sony) devices, set `freq` accordingly.

## Development

- Technical design: [docs/dev/raspir-blaster-home-assistant.md](docs/dev/raspir-blaster-home-assistant.md)
- Coding guidelines: [docs/dev/development-guidelines.md](docs/dev/development-guidelines.md)
- Specifications and changes: [openspec/](openspec/)
