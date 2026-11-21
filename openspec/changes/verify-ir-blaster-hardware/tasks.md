## 1. Preparation

- [ ] 1.1 Create `docs/dev/hardware-test-report.md` with a run template: date, firmware version + commit, Circle tag, toolchain, setup, and a result table with one row per test of the `hardware-verification` spec
- [ ] 1.2 Run `make sdcard`, copy `build/sdcard/` to a FAT32 card, and create `wpa_supplicant.conf` and `raspir-blaster.conf` with a password-protected MQTT user
- [ ] 1.3 Wire the TSOP38238 (pin 16), the KY-005 (pin 12) and the USB-serial adapter (pins 8/10), and start the serial console at 115200 baud
- [ ] 1.4 Start `mosquitto_sub -v -t 'home/ir_blaster/#'` on a LAN machine and generate the test frames from the design (`frame300.json`, carrier frames for 36/38/40 kHz)

## 2. Boot smoke test

- [ ] 2.1 Boot the device and check the serial log for the banner, "SD card mounted", the masked config line, the IP address and "Connected to broker" within 30 s; record the result

## 3. Watchdog recovery

- [ ] 3.1 Build with `make WATCHDOG_TEST=1`, copy the `kernel.img` to the card, boot, and time the hang message (≈ 30 s) and the reboot (10 s ± 2 s later); record the result
- [ ] 3.2 Restore the normal `kernel.img` (`make clean && make`) on the card and confirm the boot banner without the watchdog test

## 4. Home Assistant integration

- [ ] 4.1 Check that the device, the "IR Receiver" sensor, the model and the firmware version appear in Home Assistant and that the sensor is available
- [ ] 4.2 Stop the broker for 2 minutes and check the retry spacing in the log (5, 10, 20, 40, 60 s); start it again and check that the device reconnects and becomes available
- [ ] 4.3 Cut power while connected and check that the sensor becomes unavailable within 90 s; power up and check that it becomes available again

## 5. Capture of real remotes

- [ ] 5.1 NEC remote: press one button and check that the published frame has a 9000/4500 µs header (±10 %), 67 timings and 560 µs bit marks (±20 %)
- [ ] 5.2 RC5 remote: press one button and check that all timings are ≈ 889 or 1778 µs (±20 %); record `not run` if no RC5 remote is available
- [ ] 5.3 Air-conditioner remote: check that one button press arrives as one frame; if it is split, raise `ir_frame_gap_ms` until it is not and record the value

## 6. Carrier and timing measurement

- [ ] 6.1 With the logic analyzer on GPIO18, check that the line is low after boot and between transmissions
- [ ] 6.2 Publish the 10 ms carrier frames at 36, 38 and 40 kHz and measure frequency (±1 %) and duty cycle (33 % ± 3 %)
- [ ] 6.3 Publish `frame300.json`, export the edges, and compare every mark start/end with the running sums of `raw` (±50 µs, including the last edge)

## 7. End-to-end replay

- [ ] 7.1 Capture the TV power code, put its `raw` into a Home Assistant MQTT button, press the button, and check that the TV reacts
- [ ] 7.2 Point the receiver at the IR LED, transmit a frame, and check in `mosquitto_sub` that nothing is published to `received` because of it

## 8. Ethernet (needs a Pi 3 B or B+ and a Pi 4 B or CM4)

Moved from `add-ethernet-support` (its group 5). Use a card with `raspir-blaster.conf` and, unless stated otherwise, `wpa_supplicant.conf`; connect the board to a switch on the LAN with DHCP and the broker. QEMU pre-check results are in `add-ethernet-support/tasks.md`.

- [ ] 8.1 Pi 4 B with cable and `wpa_supplicant.conf`: Ethernet, link speed logged, MQTT connected, IR send/receive work
- [ ] 8.2 Pi 4 B without cable: marker reboot, Wi-Fi, marker deleted; power-cycle with cable: Ethernet again
- [ ] 8.3 Pi 3 B or B+ with and without cable: Ethernet / Wi-Fi after ≤ 5 s, no reboot
- [ ] 8.4 Ethernet board without `wpa_supplicant.conf` and without cable: not degraded; plugging in the cable brings MQTT up
- [ ] 8.5 Pull and re-plug the cable at runtime: MQTT reconnects with backoff, no reboot

## 9. Wrap-up

- [ ] 9.1 Complete the report entry (all tests with pass/fail/not run, measured values, environment notes) and reference a follow-up change for every failure

> **Board coverage.** These tasks were written for the Raspberry Pi Zero W (`rpi0w` target, `kernel.img`). Since `add-multi-board-builds`, the firmware also builds for the Zero 2 W and Pi 3 (`rpi3`), Pi 4 B and CM4 Lite (`rpi4`) and CM4 with eMMC (`rpi4-emmc`). None of these boards has a verification run yet. Record the board in every report entry. For each further board, repeat groups 2–7, and group 8 on the boards with Ethernet (for CM4 with eMMC, prepare `build/cm4-emmc/` instead of the SD card in 1.2), and check the boot log's "Board: …" line and the Home Assistant device model.
