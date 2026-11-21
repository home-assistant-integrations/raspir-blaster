## 1. Ring buffer (src/core)

- [x] 1.1 `src/core/logring.{h,cpp}`: `CLogRing` with `Append` (LF → CR LF), `StartReader`/`StopReader`, `Peek`/`Consume`, `TakeLost` (D1); add the header to `coreheaders.cpp`
- [x] 1.2 `tests/test_logring.cpp`: wrap-around, CR LF across append boundaries, existing CR LF kept, overrun while reading (lost count, reader at oldest), replay after restart, no reader → no lost count; `make test` passes

## 2. USB gadget (src/fw)

- [x] 2.1 `CLogUSBGadget`: DTR from `SET_CONTROL_LINE_STATE`, manufacturer/product strings with `device_id` (D3)
- [x] 2.2 `CLogTee`: UART forwarding + ring append under an IRQ-level spin lock; `Process()` with plug-and-play, `utty1` removed handler, DTR edges, lost marker, bounded send loop (D2)
- [x] 2.3 `src/fw/Makefile`: `logtee.o`, `libusbgadget.a`

## 3. Kernel integration

- [x] 3.1 Logger target = `CLogTee`; `BoardHasUSBDevice()`; gadget created and initialized only on the supported boards, failure = warning (D4)
- [x] 3.2 Call `m_LogTee.Process()` in the main loop and in the degraded and fatal loops
- [x] 3.3 `make` for all targets without warnings; check that the rpi3 image still links the host controller for Ethernet and that the Pi 3 B/B+ path never constructs the gadget
- [x] 3.4 QEMU smoke test (scratch build without watchdog arming as in `add-ethernet-support`): boot log on UART unchanged on `raspi0` and `raspi3b`, no hang when no USB host is present

## 4. Documentation

- [x] 4.1 README: "Reading the log over USB" (supported boards and ports, `screen /dev/cu.usbmodem* 115200`, replay on open, power notes, Pi 3 B/B+ and CM4 use the UART)
- [x] 4.2 `CHANGELOG.md` `[Unreleased]` entry; `CLAUDE.md`: log path (`CLogTee`, `CLogRing`) and the Circle gadget facts
- [x] 4.3 Add a "Log over USB" requirement and task group to `verify-ir-blaster-hardware` (Zero W and Pi 4 B with a Mac: boot log replay, reopen, no terminal for 10 min, Pi 3 B no USB device)
- [x] 4.4 Note the pid.codes product ID request as a release prerequisite in the README's "Making a release" section (D5)
