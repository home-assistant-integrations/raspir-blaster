## Context

The repository has no code yet. [docs/dev/raspir-blaster-home-assistant.md](../../../docs/dev/raspir-blaster-home-assistant.md) sketches the system: Circle bare-metal firmware, raw IR timings, MQTT JSON payloads, Home Assistant discovery, and reconnect logic. Its code snippets are illustrative and have not been checked against Circle's actual API. This design turns the sketch into a buildable architecture.

Fixed constraints:
- **SoC BCM2835** (Raspberry Pi Zero W): one ARM1176JZF-S core (ARMv6), `RASPPI=1`, AArch32 only, `arm-none-eabi-` toolchain, image `kernel.img`, CYW43438 Wi-Fi (`brcmfmac43430-sdio.*`).
- **Pins:** IR TX (KY-005) on GPIO18 / header pin 12, the hardware PWM0 output. IR RX (TSOP38238) on GPIO23 / header pin 16, using an edge interrupt.
- **Build host** is macOS. The toolchain and Circle are downloaded into git-ignored `toolchain/` on the first build.
- **Single core:** interrupt handlers, busy-wait transmission and the network stack all share one CPU.

## Goals / Non-Goals

**Goals:**
- Reproducible one-command build from a clean clone.
- Lossless capture and replay of arbitrary IR signals (including long air-conditioner frames) through MQTT.
- Zero-touch Home Assistant integration (discovery + availability).
- Unattended operation: recovery from network and broker outages and from hangs.
- Hardware-independent logic testable on the host.

**Non-Goals:**
- IR protocol decoding, on-device code storage, TLS, OTA updates, web UI, Wi-Fi provisioning, boards other than the Pi Zero W (see proposal).

## Decisions

### D1 — Source layout: hardware-independent core plus Circle firmware
```
src/core/   pure C++ (no Circle headers): config parser, IR message codec,
            frame assembler, carrier calculator, discovery builder, backoff
src/fw/     Circle-dependent: kernel, IR RX/TX drivers, MQTT client, config loader, Makefile
tests/      host unit tests for src/core (host clang++, `make test`)
scripts/    bootstrap (toolchain + Circle download/build)
build/      git-ignored outputs (sdcard/ directory, host test binaries)
```
`src/core` uses only freestanding C++ (fixed-size buffers, no heap, no exceptions, no RTTI, no standard library beyond `<stdint.h>`/`<stddef.h>`). It therefore compiles unchanged with Circle's flags and with the host compiler.
*Alternative:* everything in one Circle app directory, as in the design doc. Rejected because the parsers could then only be tested on hardware.

### D2 — Build: top-level Makefile + bootstrap script, Circle out of tree
The top-level `Makefile` has the targets `all`, `sdcard`, `test`, `clean` and `distclean`. `all` runs `scripts/bootstrap.sh`, which is idempotent and guarded by stamp files in `toolchain/`. The script:
1. Downloads the pinned ARM GNU Toolchain `arm-none-eabi` macOS archive (`darwin-arm64`; ARM publishes no Intel macOS build of the pinned release, so other hosts abort with a clear error), checks its SHA-256 and extracts it to `toolchain/arm-gnu-toolchain/`.
2. Clones Circle at a pinned tag into `toolchain/circle/`.
3. Writes `Config.mk` with `RASPPI = 1`, `AARCH = 32` and the absolute `PREFIX`.
4. Runs `./makeall` and then `make` in `addon/SDCard`, `addon/fatfs`, `addon/wlan` and `addon/wlan/hostap/wpa_supplicant`.

`src/fw/Makefile` sets `CIRCLEHOME = ../../toolchain/circle`, links libraries in dependency order (wlan → wpa_supplicant → SDCard → fatfs → net → sched → fs → usb/input as required → circle) and includes `Rules.mk`. The `sdcard` target runs Circle's `boot/` Makefile (Pi boot firmware) and `addon/wlan/firmware/` Makefile (Wi-Fi firmware), then copies the results into `build/sdcard/`.
The versions are pinned in `scripts/versions.env`: toolchain release + SHA-256 per host arch, and the Circle tag.
*Alternatives:* the app inside the Circle tree (`circle/app/…`, as in the design doc) was rejected because it mixes our sources with downloaded ones. Docker was rejected as heavier than needed when native macOS toolchain builds exist.

### D3 — Execution model: Circle cooperative scheduler
Circle's network stack, `CMQTTClient` and the WPA supplicant run as `CTask`s under `CScheduler`. `CKernel::Run()` is the main task. It loops over: feed the watchdog → drain completed RX frames → drain the TX queue → run the connection state machine → `Scheduler.MsSleep(…)` to yield. The main loop never busy-waits except during an IR transmission.
*Alternative:* the polling loop with `MsDelay` from the design doc. Rejected because it starves the network tasks.

### D4 — IR receive: interrupt-driven edge timestamps, double buffer
`CGPIOPin(23, GPIOModeInputPullUp, &GPIOManager)` with `EnableInterrupt(GPIOInterruptOnRisingEdge)` + `EnableInterrupt2(GPIOInterruptOnFallingEdge)` (Circle has no combined both-edges mode). The ISR reads `CTimer::GetClockTicks()` (1 MHz) and feeds the delta to the active buffer of a **double buffer** of 512 `uint16_t`. When the main loop sees that `now − lastEdge ≥ ir_frame_gap_ms`, it swaps buffers inside a short IRQ-disabled section and serializes the completed one. Capture of the next frame therefore keeps running while the previous one is published. The frame assembler logic (deltas, overflow, ≥ 4 timings, ≤ 2 s total, gap detection) lives in `src/core` and is unit-tested with synthetic timestamps. The ISR only stores timestamps.
*Alternative:* the single buffer with interrupt disable/enable from the design doc. Rejected because it loses repeat codes that arrive during publishing.

### D5 — IR transmit: PWM0 carrier, absolute deadlines, queue
- **Carrier:** PWM0 on GPIO18 (ALT5, Circle `PWM_CHANNEL1`), clocked from the 19.2 MHz oscillator with divider 2, giving a 9.6 MHz PWM clock. Range = round(9 600 000 / freq) and data = range / 3 give a 33 % duty cycle. At 36/38/40 kHz the range is 267/253/240, so the frequency error is < 0.2 %. The calculation lives in `src/core` and is tested. Circle's `CPWMOutput` fixes divider and range at construction. The transmitter therefore re-creates the `CPWMOutput` object when `freq` changes (cached per frequency). If that proves unreliable, it falls back to a thin register-level PWM driver.
- **Mark/space:** the carrier is gated by writing `data` (mark) or `0` (space). Timing uses absolute deadlines (`t0 + Σ raw[0..i]`) polled with `CTimer::GetClockTicks()`, so interrupt latency causes local jitter only and the error does not accumulate. Interrupts stay enabled, so Wi-Fi does not stall.
- **Queue:** `OnMessage()` (MQTT task) only parses and enqueues into a 4-slot ring buffer (each slot holds 512 × `uint16_t` + freq ≈ 1 KB). The main loop dequeues and transmits. A transmission lasts at most 2 s (message limit), well under the 10 s watchdog.
- **Self-capture:** before transmitting, the RX path is muted and its partial frame dropped. After the last mark, a 5 ms guard elapses before it is unmuted.
*Alternative:* bit-banging the carrier on any GPIO. Rejected because it needs 13 µs busy toggles, is jittery on a single core and wastes the hardware PWM.

### D6 — Message codec: strict, zero-allocation, hand-written
A small JSON tokenizer in `src/core` handles exactly what the schema needs: one object, number values, one integer array, and scalar unknown keys to skip. Parsing writes into a caller-provided `IRFrame { uint16_t freq_hz; uint16_t count; uint16_t raw[512]; }`; `freq_hz` is in the range 20000–60000. Validation is **strict**: out-of-range values reject the message. The design doc's clamping behavior is deliberately dropped, because a clamped AC frame or a substituted carrier would transmit a wrong command without any warning. The serializer writes the canonical compact form into a 4 KB buffer, and the worst case (512 × 5 digits) is ~3.1 KB.
*Alternative:* a general JSON library (e.g. jsmn, cJSON). Rejected: cJSON allocates, and jsmn still needs custom number and array handling. The custom tokenizer is about 150 lines and fully tested.

### D7 — Configuration: `raspir-blaster.conf` key=value on the SD card
The configuration is read once at boot via FatFs (`SD:/raspir-blaster.conf`) into a fixed 2 KB buffer and parsed by `src/core/config` into a `Config` struct with defaults. It is key=value text, not JSON or INI sections, because it is the simplest format to edit by hand on any OS and has a trivial parser. If configuration is missing or invalid, the device enters a degraded state: no MQTT, a blinking ACT LED and a fed watchdog (no reboot loop).

### D8 — MQTT: `CIRMqttClient : CMQTTClient`
The client overrides `OnConnect`, `OnDisconnect` and `OnMessage`. It is constructed with a max packet size of 5 KB so that 4 KB payloads plus topic and header fit. The connection state comes only from the callbacks. `Connect()` resolves DNS and opens the TCP connection synchronously, which can block for many seconds, so the state machine runs in the client's own task (`OnLoop()`), not in the main loop that feeds the watchdog. The state machine has the states `NoNetwork → Idle(backoff) → Connecting → Connected`, with backoff 5 s ×2 capped at 60 s (calculator in `src/core`). The Last Will is `<topic_base>/status = offline`, retained. On connect, the client publishes `online` (retained), subscribes to `<topic_base>/send` and publishes discovery (retained). All publishes use **QoS 1**: Circle's `CMQTTClient::Publish()` always appends a packet identifier, which corrupts QoS 0 PUBLISH packets (the broker would see two extra payload bytes). Received frames are not retained. Retained messages on `<topic_base>/send` are ignored, so a stale retained command is never replayed after a reconnect.
*Exact API:* constructor parameters, the `Connect()` signature and the will/retain arguments are taken from `toolchain/circle/include/circle/net/mqttclient.h` at implementation time. The design doc's snippets do not match it.

### D9 — Resilience and diagnostics
- `CBcmWatchdog` is started with a 10 s timeout and fed only in the main loop.
- `CLogger` writes to `CSerialDevice` (UART0, GPIO14/15, 115200 baud), with no HDMI/screen device, to save RAM and boot time.
- The ACT LED is the only visual status indicator: a short blink on RX/TX activity and a slow blink in the error state.

### D10 — Coding conventions
Every source file starts with the Apache-2.0 header from `docs/dev/development-guidelines.md`. Naming follows Circle's style (`C`-prefixed classes, `m_` members, `p`/`n`/`b` prefixes). `src/core` uses the same style but avoids Circle types (`u16` → `uint16_t`).

## Risks / Trade-offs

- **The Circle API differs from the design-doc snippets** → Mitigation: every Circle-facing class is written against the headers in `toolchain/circle/include` and the samples (`addon/wlan/sample`, `sample/*mqtt*`). Hardware-independent logic is isolated in `src/core`.
- **`CPWMOutput` cannot change the frequency at runtime** → Mitigation: re-create the object per frequency, or fall back to a register-level driver (D5). Covered by an early spike task.
- **15 ms default gap splits multi-part AC frames** (some brands pause 20–40 ms between sections) → Mitigation: `ir_frame_gap_ms` is configurable up to 60 ms.
- **IRQ latency (Wi-Fi/SDIO) during transmission causes edge jitter** → Mitigation: absolute deadlines prevent accumulation. The ±50 µs budget is far inside typical IR receiver tolerance (~±20 % of a 560 µs bit).
- **Driving the KY-005 directly from GPIO18 (≈ 16 mA max) limits range** → Mitigation: accepted for the first version. The design doc's NPN driver stage (2N2222, 100 Ω LED resistor) is an optional hardware upgrade with no firmware change.
- **The TSOP cannot measure the carrier, so received frames are always tagged 38 kHz** → Mitigation: the user edits `freq` in Home Assistant for 36/40 kHz devices. Most receivers tolerate a ±2 kHz carrier mismatch anyway.
- **Plain-text MQTT/Wi-Fi credentials on the SD card** → Accepted for a LAN-only device; anyone with physical access to the SD card can read them. Credentials are never logged.
- **RAM budget** (512 MB shared with the GPU): buffers are fixed-size, about 2 × 1 KB RX + 4 × 1 KB TX + 2 × 5 KB MQTT → negligible.

## Migration Plan

New project, nothing to migrate. Deployment:
1. Run `make sdcard`.
2. Copy `build/sdcard/*` to a FAT32 SD card.
3. Rename the example configs, then fill in the Wi-Fi and MQTT settings.
4. Boot the Pi.

Rollback: replace `kernel.img` with a previous build.

## Open Questions

- ~~Exact toolchain and Circle versions.~~ Resolved: ARM GNU Toolchain 15.2.rel1 (`darwin-arm64`, the version Circle's README recommends) and Circle `Step51.1.1`, pinned in `scripts/versions.env`.
- ~~Whether `CPWMOutput` supports the D5 clock setup.~~ Resolved from the Circle sources: `CPWMOutput(GPIOClockSourceOscillator, 2, range, TRUE)` works, and the object is re-created per frequency. Its behavior on hardware is still to be measured (task 12.5).
- Whether a Home Assistant `device_trigger` per received frame is useful in addition to the sensor. Deferred to a later change.
