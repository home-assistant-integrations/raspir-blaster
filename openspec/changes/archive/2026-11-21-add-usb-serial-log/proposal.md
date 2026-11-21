## Why

Reading the firmware log today needs a 3.3 V USB-serial adapter wired to header pins 8/10. Most supported boards can act as a USB device themselves: with one USB cable to a Mac, the log could appear as a serial port (`/dev/cu.usbmodem*`, no driver needed), and the same cable powers the board. This makes setup, the hardware tests and field diagnosis much easier.

## What Changes

- **Log over USB:** on boards whose USB controller can run in device mode, the firmware presents itself as a USB serial device (CDC ACM, Circle's `CUSBCDCGadget`) and writes every log line to it, in addition to UART0.
  - **Supported:** Zero W and Zero 2 W (micro-USB port marked "USB"), Pi 3 A+ (USB-A port), Pi 4 B (USB-C power port).
  - **Not used:** Pi 3 B/B+ (their USB runs through the hub/Ethernet chip and is needed as host for Ethernet) and CM4 (its USB 2 port is routed to the host controller by `otg_mode=1` in `config.txt`).
- **UART0 logging stays** unchanged on all boards.
- **Replay:** the last 16 KB of log output is kept in RAM. Each time a terminal opens the USB port (DTR set), the firmware first sends the kept log, then continues live. So the boot log is visible even if the terminal is opened later.
- **Never blocks:** if no terminal reads the port, USB output is dropped (the oldest kept text is overwritten); IR, MQTT and the watchdog are not delayed.
- **USB identity:** vendor/product ID from pid.codes (open-source VID `0x1209`), manufacturer "raspir-blaster", product "raspir-blaster (<device_id>)".
- **Ring buffer logic** in `src/core` with host unit tests.
- **Documentation:** README section "Reading the log over USB" (cable, port, `screen /dev/cu.usbmodem* 115200`, power warning), CHANGELOG.

## Capabilities

### New Capabilities
<!-- none -->

### Modified Capabilities
- `connectivity`: the "Diagnostic log on serial" requirement adds the USB serial port on the supported boards, with replay on terminal open and non-blocking output.

## Non-goals

- CM4 support for USB logging (needs a `config.txt` change that disables the host controller, and Circle does not list the CM4 as gadget-capable); may follow after a hardware test.
- Input from the USB port (commands, configuration over USB).
- Other USB functions (network over USB, mass storage for the SD card).
- Replacing UART0 logging, or changing log levels or log content.
- Logging over the network (syslog, MQTT).
- Windows host support beyond what CDC ACM gives out of the box.

## Impact

- New `src/core/logring.{h,cpp}` + `tests/test_logring.cpp`; new `src/fw/logtee.{h,cpp}` (log output device writing to UART and the USB gadget).
- `src/fw/kernel.{h,cpp}`: logger target, gadget creation on supported boards, gadget polling in the main loop; `src/fw/Makefile`: `libusbgadget.a`, new object.
- No change to `config.txt`, build targets, or Circle's configuration (vendor/product ID are constructor parameters).
- RAM: 16 KB ring buffer + Circle's 8 KB gadget queue.
- `README.md`, `CHANGELOG.md`, `CLAUDE.md`. Hardware tests (Zero W, Pi 4 B with a Mac) are added to `verify-ir-blaster-hardware`.
- A pid.codes product ID must be requested before the first release that contains this feature.
