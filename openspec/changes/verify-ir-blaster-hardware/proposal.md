## Why

The change `add-ir-blaster-firmware` is implemented and covered by host unit tests, but its timing, driver and integration behavior can only be confirmed on a real Raspberry Pi Zero W. The hardware tests were part of that change's task list, which kept the change open even though all code was done. Moving them into a separate change lets the firmware change be archived. It also turns hardware verification into a repeatable, documented procedure that can be rerun for later firmware versions.

## What Changes

- Define a hardware verification procedure for the IR blaster as its own capability. Each test has explicit pass criteria taken from the firmware specs.
- Add a test report (`docs/dev/hardware-test-report.md`) that records the setup, firmware version and the result of every test run.
- Move the six hardware tasks out of `add-ir-blaster-firmware`:
  - boot smoke test
  - watchdog reboot
  - Home Assistant availability
  - capture with real remotes
  - carrier and timing measurement
  - end-to-end replay
- No firmware behavior changes. Failures found by the tests are fixed in follow-up changes.

## Capabilities

### New Capabilities
- `hardware-verification`: on-device acceptance tests for the IR blaster firmware. It covers the setup and equipment, each test procedure with its pass criteria, and how results are recorded.

### Modified Capabilities
_None._ The firmware capabilities (`connectivity`, `ir-receive`, `ir-transmit`, `ha-discovery`, …) keep their requirements. This change only verifies them on hardware.

## Non-goals

- **Automated hardware-in-the-loop testing** (CI runners with a Pi, a robotic remote or an automated logic analyzer). All tests are manual.
- **Fixing defects.** Defects found here are recorded and handled in separate changes.
- **Range and power measurements** of the IR LED stage, and testing the optional transistor driver.
- **Long-term soak tests** (days of uptime, memory leaks).
- **Boards other than the Pi Zero W.**

## Impact

- **New docs:** `docs/dev/hardware-test-report.md`.
- **OpenSpec:** `add-ir-blaster-firmware/tasks.md` loses tasks 3.4, 10.4, 11.4, 12.5 and 12.6. Task 9.3 keeps only the watchdog implementation, which is done.
- **Equipment needed:**
  - Raspberry Pi Zero W with TSOP38238 and KY-005 wired as in the README
  - USB-serial adapter (3.3 V)
  - MQTT broker with Home Assistant
  - Logic analyzer or oscilloscope (≥ 10 MHz sample rate)
  - NEC, RC5 and air-conditioner remotes
  - A TV or other IR-controlled device
- **Firmware build:** uses the existing `make WATCHDOG_TEST=1` test image; no code changes.
- **Boards:** the tests target the Raspberry Pi Zero W. The boards added by `add-multi-board-builds` (Zero 2 W, Pi 3, Pi 4 B, CM4 Lite, CM4 with eMMC) still need their own runs of groups 2–7 (see the note in `tasks.md`).
