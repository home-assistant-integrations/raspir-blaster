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

| Board                             | Image              | Boot media     | Network         | Status                  |
| --------------------------------- | ------------------ | -------------- | --------------- | ----------------------- |
| Raspberry Pi Zero W               | `kernel.img`       | SD card        | Wi-Fi           | built, not yet verified |
| Raspberry Pi Zero 2 W             | `kernel8.img`      | SD card        | Wi-Fi           | built, not yet verified |
| Raspberry Pi 3 A+                 | `kernel8.img`      | SD card        | Wi-Fi           | built, not yet verified |
| Raspberry Pi 3 B / B+             | `kernel8.img`      | SD card        | Ethernet, Wi-Fi | built, not yet verified |
| Raspberry Pi 4 B                  | `kernel8-rpi4.img` | SD card        | Ethernet, Wi-Fi | built, not yet verified |
| Compute Module 4 Lite             | `kernel8-rpi4.img` | SD card        | Ethernet, Wi-Fi | built, not yet verified |
| Compute Module 4 with eMMC        | `kernel8-rpi4.img` | eMMC           | Ethernet, Wi-Fi | built, not yet verified |
| Compute Module 4 without wireless | `kernel8-rpi4.img` | SD card / eMMC | Ethernet        | built, not yet verified |

"Verified" means the hardware tests of the OpenSpec change
`verify-ir-blaster-hardware` passed on that board. On a CM4, Ethernet needs a
carrier board with an Ethernet jack.

Not supported: Raspberry Pi 5, Compute Module 5, Pi 400, Pi 1, Pi 2, Pi Zero
(without W), and CM3/CM3+. The Pi 5's RP1 I/O chip needs different drivers.

### Ethernet or Wi-Fi

The firmware uses one network interface per boot and chooses it at power-up:

- **Boards without Ethernet** (Zero W, Zero 2 W, 3 A+) always use Wi-Fi and
  need `wpa_supplicant.conf`.
- **Boards with Ethernet and without `wpa_supplicant.conf`** always use
  Ethernet. They wait for a cable if none is connected.
- **Boards with Ethernet and `wpa_supplicant.conf`** use Ethernet if the link
  comes up within 5 s, otherwise Wi-Fi:
  - Pi 3 B/B+ decide during the boot, without a reboot.
  - Pi 4 B and CM4 can only check the link by starting the network on
    Ethernet. Without a link they create the file `raspir-blaster.wifi` on the
    card and reboot once; the next boot deletes the file and uses Wi-Fi. The
    boot after that tries Ethernet again.

Plugging in or pulling the cable later takes effect at the next boot. The two
interfaces have different MAC addresses, so your DHCP server may give the
device a different IP address on each; only the broker address matters to the
firmware. The serial log names the chosen interface, e.g.
`Network: Ethernet (link up, 1000BASE-T Full-duplex)` or
`Network: Wi-Fi (no Ethernet link)`.

A CM4 without wireless that has a `wpa_supplicant.conf` and no cable keeps
rebooting (the Wi-Fi attempt fails); every second boot tries Ethernet again, so
connecting the cable or deleting `wpa_supplicant.conf` ends this.

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

### Reading the log over USB

On most boards the log is also available over a single USB cable, without a
serial adapter. The board then appears on the computer as a USB serial port
(CDC ACM, no driver needed on macOS and Linux), and the same cable powers it.

| Board            | Port to connect to the computer        |
| ---------------- | -------------------------------------- |
| Zero W, Zero 2 W | the micro-USB port marked "USB" (not "PWR IN") |
| Pi 3 A+          | the USB-A port (USB-A to USB-A or USB-C cable) |
| Pi 4 B           | the USB-C power port                   |

On a Mac:

```sh
ls /dev/cu.usbmodem*                 # appears a few seconds after boot
screen /dev/cu.usbmodem* 115200      # the baud rate is ignored over USB
```

Each time a terminal opens the port, it first shows the last 16 KB of log, so
the boot log is visible even if you connect later; then it follows the live
log. Without an open terminal nothing is sent and the device works as usual.
If the terminal cannot keep up, a line `[log: <n> bytes lost]` marks the gap.
The UART log is not affected.

- Power the board **only** through this USB cable while logging over USB.
  Circle warns that a second supply with a different voltage can damage the
  board.
- A Pi 4 B powered by a Mac's USB-C port has enough power without USB
  peripherals; if it resets, use a powered USB hub.
- Pi 3 B/B+ and CM4 only log on the UART: the Pi 3 B/B+ USB ports are host
  ports (used for Ethernet), and on the CM4 `otg_mode=1` connects the USB 2
  port to the host controller.

## Installation

You need a micro SD card (any size; the files take about 10 MB), or for a CM4
with eMMC the [`rpiboot`](https://github.com/raspberrypi/usbboot) tool, and
an MQTT broker that Home Assistant uses (e.g. the Mosquitto add-on).

### 1. Download

Download the zip for your board from the latest release on the
[Releases page](https://github.com/home-assistant-integrations/raspir-blaster/releases):

| Board | File |
| --- | --- |
| Zero W, Zero 2 W, Pi 3 A+/B/B+, Pi 4 B, CM4 Lite | `raspir-blaster-<version>-sdcard.zip` |
| CM4 with eMMC | `raspir-blaster-<version>-cm4-emmc.zip` |

The same SD card zip works for every board in the first row: its
`config.txt` starts the right image for the board it is in. Optionally,
verify the download with `SHA256SUMS.txt` from the same release, in the
folder with the zip:

```sh
shasum -a 256 -c SHA256SUMS.txt   # "OK" for your zip; the other zip is reported missing
```

### 2. Prepare the SD card

1. Format the card with a single FAT32 partition. On a Mac, find the card
   with `diskutil list` (check the size: this erases the disk), then e.g.
   ```sh
   diskutil eraseDisk FAT32 RBLASTER MBRFormat /dev/disk4
   ```
2. Extract the zip and copy its **contents** (`config.txt`, `kernel*.img`,
   `start*.elf`, `firmware/`, ...) to the top level of the card, not into a
   subfolder.

### 3. Configure

On the card:

1. Rename `raspir-blaster.conf.example` to `raspir-blaster.conf` and set at
   least `mqtt_host`, plus `mqtt_user` and `mqtt_password` if your broker
   needs them (all keys: [Configuration](#configuration-raspir-blasterconf)):
   ```ini
   mqtt_host=192.168.1.100
   mqtt_user=irblaster
   mqtt_password=secret
   ```
2. For Wi-Fi, rename `wpa_supplicant.conf.example` to `wpa_supplicant.conf`
   and enter your network name and password in `ssid` and `psk` (and your
   country code in `country`). This file is required on the Zero W,
   Zero 2 W and Pi 3 A+. On a Pi 3 B/B+, Pi 4 B or CM4 that you connect by
   Ethernet, leave it out (see [Ethernet or Wi-Fi](#ethernet-or-wi-fi)).

### 4. Start

1. Eject the card, insert it into the Pi, connect the IR modules (see
   [Hardware](#hardware)) and power the Pi. With Ethernet, plug in the cable
   before powering up.
2. Within about 30 s the device appears in Home Assistant under
   **Settings → Devices & services → MQTT** as "raspir-blaster
   (`<device_id>`)" with an "IR Receiver" sensor (see
   [Home Assistant](#home-assistant)).

If it does not appear, the ACT LED tells you why:

| ACT LED | Meaning |
| --- | --- |
| short flash on IR activity, otherwise off | running |
| slow blink (1 s on, 1 s off) | configuration missing/invalid, or setup mode (see below) |
| fast blink | hardware or driver error; the device reboots after 10 s |

The log names the problem: over USB on the Zero W, Zero 2 W, Pi 3 A+ and
Pi 4 B (see [Reading the log over USB](#reading-the-log-over-usb)), or with a
serial adapter on UART0 on every board.

**Setup mode.** Instead of editing the two files by hand, boards with a
Wi-Fi chip can be configured over the air: boot the card with
`raspir-blaster.conf.example` left unrenamed (or missing), and the device
opens an open Wi-Fi access point and a configuration API for 30 minutes —
see [Setup mode](docs/api.md#setup-mode) in the API docs. A CM4 without
wireless has no setup mode and stays in the slow-blink state until
`raspir-blaster.conf` is added by hand.

### Compute Module 4 with eMMC

A CM4 with eMMC boots from its eMMC instead of an SD card:

1. Set the carrier board's boot jumper to USB boot (e.g. "Fit jumper to
   disable eMMC Boot" on the CM4 IO Board), connect its USB port to the Mac
   and run `rpiboot`. The eMMC then appears as a USB drive.
2. Make sure the eMMC has a FAT32 partition, then copy the **contents** of
   the `cm4-emmc` zip to it.
3. Configure the two files as in step 3.
4. Remove the jumper and power-cycle the module.

### Updating

**Over the API** (needs `ota_password` set in `raspir-blaster.conf`): upload
the release zip for your board directly to the running device — see
[`docs/api.md`](docs/api.md) for the full `POST /api/update` request and
response. The device validates the zip, installs the new kernel image and
reboots on its own, keeping the previous image as `<image>.bak` on the card.

```sh
curl --form-string password=<ota_password> -F file=@raspir-blaster-<version>-sdcard.zip \
     http://<device-ip-or-hostname>/api/update
```

**Manually:** copy the contents of the new zip over the old files on the
card, but keep your `raspir-blaster.conf` and `wpa_supplicant.conf` (the zip
only contains the `.example` files, so they are not overwritten).

If a new kernel does not start, rename `<image>.bak` back to `<image>` on
the card in a computer to restore the previous firmware (either update
path keeps this backup).

### Building the card yourself

`make sdcard` (after `make` for all targets, see [Building](#building))
creates the same contents in `build/sdcard/` and `build/cm4-emmc/`. Use them
instead of the zips in steps 2 to 4.

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

## Releases

Releases are published on the
[Releases page](https://github.com/home-assistant-integrations/raspir-blaster/releases).
Each release has:

| File | For |
| --- | --- |
| `raspir-blaster-<version>-sdcard.zip` | Zero W, Zero 2 W, Pi 3, Pi 4 B, CM4 Lite: extract onto a FAT32 SD card |
| `raspir-blaster-<version>-cm4-emmc.zip` | CM4 with eMMC: copy onto the eMMC (see [Installation](#compute-module-4-with-emmc)) |
| `SHA256SUMS.txt` | checksums; verify with `shasum -a 256 -c SHA256SUMS.txt` |

**Version scheme:** `yyyy.mm.rm`, with the year and two-digit month of the
release (UTC) and a release counter `rm` that starts at `0` every month, e.g.
`2026.11.0`, `2026.11.1`, `2026.12.0`. The firmware reports its version in the
serial boot log and as the firmware version of the Home Assistant device.
Builds outside a release report `dev`.

### Making a release

Every pull request with a user-visible change adds an entry under
`## [Unreleased]` in [CHANGELOG.md](CHANGELOG.md) (format:
[Keep a Changelog 1.1.0](https://keepachangelog.com/en/1.1.0/), categories
`### Added`, `### Changed`, `### Fixed`, ...).

Before the first release with the USB serial log: request an own USB product
ID from [pid.codes](https://pid.codes) (vendor `0x1209`) and set
`USB_LOG_PRODUCT_ID` in `src/fw/logtee.h`. Until then the firmware uses the
pid.codes test ID `0x0001`, which must not be used in distributed devices.

Maintainers start a release under **Actions → Release → Run workflow** on
`main`. The workflow:

1. computes the next version,
2. moves the `[Unreleased]` entries of `CHANGELOG.md` into a section
   `## [<version>] - <date>` and updates the compare links at the end of the
   file,
3. creates the branch `rel-<version>` with one commit that adds a `VERSION`
   file and contains the updated `CHANGELOG.md`,
4. builds and tests the firmware on that branch,
5. publishes the GitHub release `<version>` with the files above. The release
   notes show the download table, the version's changelog section and
   GitHub's generated list of pull requests.

The workflow stops before creating the release branch if `[Unreleased]` has no
entries, or if `CHANGELOG.md` on `main` does not contain the previous release
yet. If a later step fails, the workflow deletes the release branch again, so
the version number is used by the next run. Only one release runs at a time.

The workflow never writes to `main`. After each release, bring the released
changelog back to `main` with a pull request, **before the next release**
(the next release refuses to start until then):

```sh
git fetch origin
git switch -c changelog-<version> origin/main
git diff origin/rel-<version>~1 origin/main -- CHANGELOG.md   # entries merged since the release started
git checkout origin/rel-<version> -- CHANGELOG.md
```

If the `git diff` shows new entries (pull requests merged after the release
started), copy them back under `## [Unreleased]`: the released file only has
an empty `[Unreleased]`, so without this step those entries are lost. Then:

```sh
git commit -am "docs(changelog): <version>"
git push -u origin changelog-<version>   # then open the pull request
```

Only copy `CHANGELOG.md`; never merge `rel-<version>` into `main`, because
that would bring the `VERSION` file along.

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
| `device_id`       | no       | `pi-zero-ir`      | 1–31 chars of `a-z`, `0-9`, `-`             |
| `topic_base`      | no       | `home/ir_blaster` | 1–63 chars, no `+`, `#` or trailing `/`     |
| `ir_frame_gap_ms` | no       | `15`              | 5–60                                        |
| `ota_password`    | no       | unset (updates disabled) | 8–63 chars                           |

`device_id` is the MQTT client ID, the DHCP host name, the mDNS name and the
Home Assistant device identifier — used as-is everywhere, which is why it
may not contain `_` (not a valid host name character). Give every blaster
on the same broker its own `device_id` and `topic_base`.

`ota_password` enables the firmware update API (see
[Updating](#updating) and [`docs/api.md`](docs/api.md)); without it,
`POST /api/update`, `POST /api/update/confirm` and `POST /api/restart`
are refused. It is never shown in logs or API responses, only whether it
is set.

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
