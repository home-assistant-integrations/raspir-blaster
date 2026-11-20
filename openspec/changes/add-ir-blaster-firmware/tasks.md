## 1. Repository setup

- [ ] 1.1 Create the directory skeleton `src/core/`, `src/fw/`, `tests/`, `scripts/` and `config/`, and add `build/` to `.gitignore`
- [ ] 1.2 Add `scripts/versions.env` that pins the ARM GNU Toolchain release (`arm-none-eabi`), the SHA-256 for `darwin-arm64` and `darwin-x86_64`, and the Circle git tag (latest release and the toolchain its README recommends)
- [ ] 1.3 Add the example files `config/raspir-blaster.conf.example` (all keys from the device-config spec, with comments) and `config/wpa_supplicant.conf.example` (placeholder SSID/PSK, `country=DE`)

## 2. Toolchain bootstrap

- [ ] 2.1 `scripts/bootstrap.sh`: detect the host arch, download the pinned toolchain archive into `toolchain/`, verify its SHA-256 (abort on mismatch before extracting), extract it, and write a stamp file
- [ ] 2.2 `scripts/bootstrap.sh`: clone Circle at the pinned tag into `toolchain/circle/` (skip if the stamp exists) and write `Config.mk` with `RASPPI = 1`, `AARCH = 32` and the absolute `PREFIX`
- [ ] 2.3 `scripts/bootstrap.sh`: run `./makeall` and build the addons `SDCard`, `fatfs`, `wlan` and `wlan/hostap/wpa_supplicant`, guarded by a stamp file; check that a second run is a no-op
- [ ] 2.4 Top-level `Makefile` with targets `all` (bootstrap + firmware), `clean` (keeps `toolchain/`) and `distclean` (removes `toolchain/`)

## 3. Minimal firmware image

- [ ] 3.1 `src/fw/Makefile`: `CIRCLEHOME`, the library list in link order, the addon include paths and `include Rules.mk`
- [ ] 3.2 `src/fw/main.cpp` and `kernel.{h,cpp}` that initialize the interrupt system, timer, serial logger (UART0, 115200) and scheduler, and log a boot banner with the firmware version; `make` produces `kernel.img`
- [ ] 3.3 `sdcard` target: run Circle's `boot/` and `addon/wlan/firmware/` downloads, then copy the boot files, `kernel.img`, `firmware/brcmfmac43430-sdio.*` and the example configs into `build/sdcard/`
- [ ] 3.4 Hardware smoke test: boot `build/sdcard` on the Pi Zero W and confirm the banner on the serial console

## 4. Host test harness

- [ ] 4.1 `tests/` mini test framework (assert macros, test registry, non-zero exit on failure) and a `make test` target that builds `src/core/*.cpp` + `tests/*.cpp` with host `clang++`
- [ ] 4.2 Compile `src/core` with both the host compiler and the Circle flags (an empty translation unit including all core headers) to enforce the "no Circle and no libc++ dependencies" rule

## 5. Core: configuration parser

- [ ] 5.1 Tests + implementation: `key=value` line parsing with whitespace trim, `#` comments, blank lines and LF/CRLF endings
- [ ] 5.2 Tests + implementation: `Config` struct with defaults, a validation table for every key (ranges, character sets, lengths), and warnings for unknown or invalid keys
- [ ] 5.3 Tests + implementation: missing `mqtt_host` → invalid config; a config dump function that masks `mqtt_password`

## 6. Core: IR message codec

- [ ] 6.1 Tests + implementation: tokenizer for objects, integers, strings, booleans/null and arrays, with whitespace handling
- [ ] 6.2 Tests + implementation: `ParseIRMessage()` reading `raw` + optional `freq` (default 38000), any key order, unknown scalar keys skipped
- [ ] 6.3 Tests + implementation: rejection rules (payload > 4096 B, `raw` missing/empty/> 512, element outside 1–65535, total > 2 s, `freq` outside 20000–60000, malformed JSON), each with a distinct reason string
- [ ] 6.4 Tests + implementation: `SerializeIRMessage()` in canonical compact form; a round-trip test; a worst-case size test (512 × 5 digits fits in 4096 B)

## 7. Core: frame assembler, carrier, backoff, discovery

- [ ] 7.1 Tests + implementation: frame assembler fed with edge timestamps: deltas, gap detection with a configurable gap, trailing gap excluded, < 4 timings discarded, > 512 timings or > 2 s → overflow, wrap-around of the 32-bit tick counter
- [ ] 7.2 Tests + implementation: carrier calculator (9.6 MHz PWM clock → range, data at 33 %); error < 1 % for 20–60 kHz
- [ ] 7.3 Tests + implementation: reconnect backoff 5 → 10 → 20 → 40 → 60 → 60 s, with reset on success
- [ ] 7.4 Tests + implementation: discovery topic and payload builder using `device_id`/`topic_base`/version; check the payload against the ha-discovery spec fields

## 8. Firmware: storage and configuration

- [ ] 8.1 Initialize EMMC and mount FatFs on `SD:` (following `addon/wlan/sample`), and log the result
- [ ] 8.2 Load `SD:/raspir-blaster.conf` into a buffer, parse it with the core parser and log the masked config
- [ ] 8.3 Degraded state: on a missing or invalid config or a missing `wpa_supplicant.conf`, the ACT LED does a slow blink, there is no MQTT, and the main loop keeps running

## 9. Firmware: network and watchdog

- [ ] 9.1 Initialize `CBcm4343Device` (`SD:/firmware/`), `CNetSubSystem` (DHCP, host name = `device_id`) and `CWPASupplicant`; log the IP address once it is up
- [ ] 9.2 Main loop skeleton under the scheduler: feed the watchdog, run the drain hooks, then `MsSleep` to yield
- [ ] 9.3 Start `CBcmWatchdog` with a 10 s timeout and feed it only in the main loop; test on hardware by forcing a hang in a debug build and confirming the reboot

## 10. Firmware: MQTT

- [ ] 10.1 Read `circle/net/mqttclient.h` and write `CIRMqttClient` with `OnConnect`/`OnDisconnect`/`OnMessage` stubs and a 5 KB max packet size
- [ ] 10.2 Connection state machine (`NoNetwork`/`Idle`/`Connecting`/`Connected`) using the core backoff, credentials and Last Will `offline` (retained)
- [ ] 10.3 On connect: publish `online` (retained), subscribe to `<topic_base>/send`, publish discovery (retained); log every state transition
- [ ] 10.4 Hardware test: the device appears in Home Assistant as available; stopping the broker → unavailable; starting it again → reconnect and available

## 11. Firmware: IR receive

- [ ] 11.1 Configure GPIO23 via `CGPIOManager` with an interrupt on both edges; the ISR stores tick timestamps in the active half of the double buffer
- [ ] 11.2 Main loop: detect frame completion (gap), swap buffers in an IRQ-disabled section, run the frame assembler and serialize the frame
- [ ] 11.3 Publish completed frames to `<topic_base>/received` (QoS 0, not retained) only while connected; flash the ACT LED on each frame
- [ ] 11.4 Hardware test: press NEC, RC5 and AC remote buttons and confirm that sensible frames show up in the Home Assistant sensor attributes

## 12. Firmware: IR transmit

- [ ] 12.1 Spike: check whether `CPWMOutput` supports oscillator clock + divider 2 + a variable range on GPIO18; if not, write a minimal register-level PWM0 driver
- [ ] 12.2 `CIRTransmitter`: set the carrier for `freq` (cached), gate the mark/space with absolute deadlines, and keep GPIO18 low when idle
- [ ] 12.3 TX ring buffer (4 slots): `OnMessage` parses and enqueues (and logs drops or rejections), and the main loop dequeues and transmits
- [ ] 12.4 RX muting: drop the partial RX frame before transmitting, ignore edges during TX, and unmute after a 5 ms guard
- [ ] 12.5 Hardware test: measure the carrier frequency and duty cycle at 36/38/40 kHz with a logic analyzer or scope, and the edge timing of a 300-timing frame (±50 µs)
- [ ] 12.6 End-to-end test: capture a TV power code, paste it into a Home Assistant MQTT button, press the button, and confirm the TV reacts and no self-capture is published

## 13. Documentation

- [ ] 13.1 Extend `README.md`: wiring table (GPIO23/pin 16, GPIO18/pin 12), build and `make sdcard` steps, SD card setup, configuration keys, and the Home Assistant learning workflow
- [ ] 13.2 Update the design doc and `CLAUDE.md` with any Circle API corrections found during implementation (MQTT client, PWM, GPIO manager)
