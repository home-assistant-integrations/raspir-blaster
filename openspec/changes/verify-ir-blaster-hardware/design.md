## Context

The firmware from `add-ir-blaster-firmware` builds cleanly and its hardware-independent logic is covered by 82 host unit tests. Not yet verified are:
- the Circle drivers on a real BCM2835 (SDHOST, WLAN, PWM, GPIO interrupts, watchdog)
- timing accuracy under real interrupt load
- the interaction with Home Assistant and real remotes

These tests need physical equipment and a person, so they live in their own change and are run manually.

## Goals / Non-Goals

**Goals:**
- A repeatable procedure with objective pass criteria, derived from the firmware specs.
- A written record of every run, so later firmware versions can be compared.

**Non-Goals:**
- Test automation, defect fixing, range/power measurements, soak tests (see proposal).

## Decisions

### D1 — Manual procedure, results in a Markdown report
Tests are run by hand following `tasks.md`. Results go into `docs/dev/hardware-test-report.md`, one section per run with a result table. Newest run comes first.
*Alternative:* a hardware-in-the-loop CI rig. Rejected as far too much effort for a single-device hobby project.

### D2 — Test setup
- **Device:** Pi Zero W with TSOP38238 on GPIO23 / header pin 16 and KY-005 on GPIO18 / header pin 12, wired as in the README.
- **USB log tests:** the Zero W and a Pi 4 B, each powered only by a USB cable to a Mac (Zero W: port marked "USB"; Pi 4 B: USB-C), terminal `screen /dev/cu.usbmodem* 115200`.
- **Ethernet tests:** a Pi 3 B or B+ and a Pi 4 B or CM4 (carrier board with an Ethernet jack), wired the same way for the IR checks, on a switch in the same LAN as the broker and the DHCP server.
- **Serial log:** a 3.3 V USB-serial adapter on header pins 8 (TX) and 10 (RX), 115200 baud, e.g. `screen /dev/tty.usbserial-* 115200`.
- **MQTT:** the Home Assistant Mosquitto add-on. Messages are observed and injected with `mosquitto_sub -v -t 'home/ir_blaster/#'` and `mosquitto_pub -t home/ir_blaster/send -f frame.json`, from a machine on the same LAN.
- **Signal measurement:** a logic analyzer (≥ 10 MHz sample rate, e.g. a Saleae-compatible device with sigrok/PulseView) clipped to GPIO18 and GND. At 10 MHz one sample is 0.1 µs, small enough for the ±1 % carrier and ±50 µs edge criteria. The duty cycle is read from the PWM pulse widths.

### D3 — Synthetic test frames
Carrier and timing tests use generated frames, so the expected edge times are known exactly:
- *Carrier test:* `{"freq":F,"raw":[10000]}` for F = 36000, 38000, 40000. This gives a single 10 ms mark, long enough to average the frequency over ~380 periods.
- *Timing test:* 300 timings alternating 1000/1666 µs (≈ 400 ms total). Generated with a one-line script, for example:
  `python3 -c 'import json;print(json.dumps({"freq":38000,"raw":[1000 if i%2==0 else 1666 for i in range(300)]}))' > frame300.json`
  The nominal edge times are the running sums of `raw`. They are compared against the edges exported from the logic analyzer, relative to the first rising edge of the carrier.

### D4 — Pass criteria come from the firmware specs
Each scenario in `specs/hardware-verification` cites a limit that is already defined in the firmware specs (±1 % carrier, 33 % ± 3 % duty, ±50 µs edges, 5 s → 60 s backoff, 10 s watchdog). No new limits are introduced. For the remote tests, which have no limit in the firmware specs, generous protocol tolerances (±10–20 %) are used, because consumer remotes vary.

## Risks / Trade-offs

- **[Measurement probe disturbs the signal]** → Measure at GPIO18 with the KY-005 connected. The logic analyzer input is high impedance, and the probe ground shares the Pi's ground.
- **[No RC5 or AC remote available]** → Record the test as `not run` with the reason. These tests are not blocking for archiving this change, but missing tests must be listed in the report.
- **[Results depend on the environment]** (sunlight, fluorescent lights disturbing the TSOP) → Run the receive tests indoors without direct sunlight and note the conditions in the report.
- **[The watchdog test image left on the card]** → The last watchdog task restores the normal `kernel.img`. The report records which image was used for each test.

## Migration Plan

Not applicable. This change adds no code. To start: `make sdcard`, prepare the card, wire the device and follow `tasks.md`.

## Open Questions

- Which air-conditioner remote is available, and does it need a larger `ir_frame_gap_ms` than 15 ms? This is answered by the test and recorded in the report.
