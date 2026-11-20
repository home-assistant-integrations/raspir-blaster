## Why

Home Assistant has no way to control infrared-only devices (TVs, air conditioners, audio gear) or to react to physical IR remotes without a dedicated bridge. Linux-based Pi solutions (LIRC) need a full OS, slow boots and SD-card maintenance. A bare-metal Circle firmware on a Raspberry Pi Zero W boots in seconds, has microsecond-accurate timing for IR and needs no OS upkeep. It is the first functional change for this repository, which has no code yet.

## What Changes

- Add a reproducible build that downloads the ARM GNU toolchain (`arm-none-eabi-`, macOS host) and the Circle sources into `toolchain/` on first build. It then builds Circle and its addons for the BCM2835 (`RASPPI=1`, AArch32) and produces `kernel.img`.
- Add firmware that brings up the SD card, onboard Wi-Fi (CYW43438), DHCP and an MQTT connection to the Home Assistant broker.
- Read device settings (MQTT broker host, port, credentials, device id) from a config file on the SD card. No secrets are compiled in or committed.
- Capture IR signals from a TSOP38238 on GPIO23 as raw mark/space timings and publish each complete frame to MQTT.
- Transmit raw IR timings received via MQTT through the KY-005 on GPIO18. The carrier comes from hardware PWM, with the frequency set per message.
- Define one JSON message format (`{"freq":38000,"raw":[...]}`) used in both directions, so a captured code can be replayed unchanged.
- Register the device in Home Assistant through MQTT Discovery, with an availability topic driven by Last Will and Testament.
- Recover automatically from Wi-Fi and broker outages (reconnect with backoff) and from firmware hangs (hardware watchdog).

## Capabilities

### New Capabilities
- `firmware-build`: toolchain and Circle download into `toolchain/`, Circle and addon build, kernel image build, SD card file layout.
- `device-config`: SD-card config file format, required and optional keys, defaults, behavior when the file is missing or invalid.
- `connectivity`: Wi-Fi and DHCP bring-up, MQTT connection with credentials and LWT, reconnect with backoff, watchdog.
- `ir-message-format`: JSON payload schema for raw IR frames, parsing rules, limits (payload size, timing count, frequency range), handling of malformed input.
- `ir-receive`: edge capture on GPIO23, frame segmentation by silence timeout, buffer limits, publishing of received frames.
- `ir-transmit`: carrier generation on GPIO18 via hardware PWM, mark/space playback, receiver suppression during transmit.
- `ha-discovery`: MQTT Discovery config payloads, device metadata, availability topic, retained publishing on every connect.

### Modified Capabilities
_None. There are no existing specs._

## Non-goals

- **Protocol decoding** (NEC, Sony, RC5/RC6, AC brands). The firmware works only with raw timings. Interpretation, if ever needed, happens in Home Assistant.
- **Storing IR codes on the device.** Codes live in Home Assistant (buttons, scripts, automations).
- **Other boards or 64-bit builds.** Only the Raspberry Pi Zero W (BCM2835) is supported.
- **Ethernet, Bluetooth, USB peripherals, display output.**
- **TLS for MQTT** and MQTT 5 features. Plain MQTT 3.1.1 on the local network only.
- **Over-the-air firmware updates** and any web or configuration UI.
- **Wi-Fi provisioning** (AP mode, captive portal). Credentials are written to the SD card by hand.

## Impact

- **New code:** firmware sources (kernel, IR receiver, IR transmitter, payload parser, MQTT client subclass, config reader), the application `Makefile` and a bootstrap or build script.
- **Dependencies:** ARM GNU toolchain (`arm-none-eabi`) and Circle with the `SDCard`, `fatfs`, `wlan` and `wpa_supplicant` addons, plus Raspberry Pi boot firmware and CYW43438 Wi-Fi firmware for the SD card. All are downloaded at build time and not committed.
- **Hardware:** TSOP38238 signal on GPIO23 (header pin 16), KY-005 signal on GPIO18 (header pin 12).
- **External systems:** a Home Assistant MQTT broker (Mosquitto). The new MQTT topics are `home/ir_blaster/send`, `home/ir_blaster/received`, `home/ir_blaster/status` and `homeassistant/…/pi_zero_ir/…`.
- **Docs:** the design doc and CLAUDE.md are already aligned to the BCM2835 and the pin assignments above.
