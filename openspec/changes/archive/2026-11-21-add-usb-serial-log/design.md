## Context

`CKernel` initializes `CLogger` with the UART (`m_Logger.Initialize (&m_Serial)`); all firmware and Circle log output goes through `CLogger::Write`, which calls `m_pTarget->Write ()` (outside its spin lock, possibly from interrupt context in Circle drivers).

Relevant Circle facts (Step51.1.1, verified in `toolchain/circle`):
- `CUSBCDCGadget (pInterruptSystem, usVendorID, usProductID)` derives from `CDWUSBGadget`; `doc/classes.txt` and `test/usb-serial-cdc-gadget/README` name the supported boards: Pi (3) A(+), Zero (2) (W), 4B (USB-C power connector). The vendor/product ID are constructor parameters, so Circle's libraries need no rebuild. Vendor ID `0x0000` is rejected.
- After the host configured the device, `UpdatePlugAndPlay()` (to be polled) creates a `CUSBSerialDevice` named `utty1`. Its `Write()` is non-blocking: `CUSBCDCGadgetEndpoint` copies into an 8 KB queue and returns the number of bytes accepted (0 if full). With `SERIAL_OPTION_ONLCR` the return value counts converted bytes, which breaks partial-write accounting.
- `CDWUSBGadget::OnClassOrVendorRequest()` is virtual; the CDC gadget does not override it, so OUT class requests (`SET_LINE_CODING`, `SET_CONTROL_LINE_STATE`) are accepted and ignored, and DTR is not visible. `GetDescriptor()` is virtual ("may override to personalize"); string 1 = manufacturer, 2 = product, no serial number.
- On the Pi 3 B/B+ the DWC controller is behind the LAN9514/LAN7515 hub and is used as host (Ethernet, `add-ethernet-support`). `config.txt` sets `otg_mode=1` for the CM4, routing its USB 2 port to the xHCI host controller.

## Goals / Non-Goals

**Goals:**
- One USB cable to a Mac shows the full log, including the boot log, without extra hardware.
- No effect on timing: IR, MQTT and the watchdog never wait for USB.
- No change to UART logging, `config.txt`, Circle's configuration or the Pi 3 B/B+ Ethernet path.

**Non-Goals:**
- See proposal (CM4, input over USB, other USB functions, network logging).

## Decisions

### D1 — Ring buffer with reader cursor in `src/core/logring.{h,cpp}`
`CLogRing` (16 KB static buffer) stores the log text with `\n` converted to `\r\n` on append, so USB writes are byte-exact and partial writes need no conversion bookkeeping. It tracks the total number of bytes written and, while a reader is active, a reader position:
- `Append (pData, nLength)` — copies with wrap-around; if the active reader falls more than the capacity behind, it is moved to the oldest kept byte and the skipped count is added to a lost counter.
- `StartReader ()` — reader at the oldest kept byte, lost counter 0; `StopReader ()`.
- `Peek (&pData)` → length of the contiguous block at the reader (up to the wrap point); `Consume (nLength)`.
- `TakeLost ()` → lost bytes since the last call.
Host tests cover wrap-around, CRLF conversion across append boundaries, overrun while reading, and start/stop.
*Alternative:* use `CLogger`'s own 16 KB text ring (`CLogger::Read`). Rejected: reading clears it (no replay on the next terminal open), and it has no reader cursor or lost count.

### D2 — `CLogTee` as logger target (`src/fw/logtee.{h,cpp}`)
A `CDevice` whose `Write()` forwards to the UART unchanged and appends to the `CLogRing` under a `CSpinLock` at IRQ level (the logger may be called from interrupt handlers; the critical section is one bounded `memcpy`). It never touches USB. `m_Logger.Initialize (&m_LogTee)` replaces `&m_Serial`.
`Process()` — called from the main task — does the USB side: poll `UpdatePlugAndPlay()`, pick up `utty1` (and register its removed handler), start the reader on a DTR rising edge, stop it on DTR falling or device removal, write a `[log: <n> bytes lost]` line when `TakeLost()` is non-zero, then send `Peek`/`Write`/`Consume` until the gadget queue is full or the ring is empty. At most one pass per call, so the main loop is never held up.

### D3 — DTR from a small gadget subclass
`CLogUSBGadget : public CUSBCDCGadget` overrides:
- `OnClassOrVendorRequest()`: for `bmRequestType` class/interface/OUT and `bRequest 0x22` (`SET_CONTROL_LINE_STATE`) it stores `wValue & 1` in a `volatile boolean`; everything is then delegated to the base class. Runs in interrupt context, so it only sets the flag.
- `GetDescriptor()`: string 1 → "raspir-blaster", string 2 → "raspir-blaster (<device_id>)" via `ToStringDescriptor()`; everything else from the base class.
*Alternative:* send whenever `utty1` exists. Rejected: macOS discards data while no process has the port open, so the boot log would be lost and replay would have no trigger.

### D4 — Boards and lifetime
`BoardHasUSBDevice()` in `src/fw`: `MachineModelZeroW`, `MachineModelZero2W`, `MachineModel3APlus`, `MachineModel4B`. Only then is `CLogUSBGadget` created (`new`, after the timer) and `Initialize()`d; a failure is logged as a warning and logging continues on UART only (not fatal). On the Pi 3 B/B+ the gadget is never constructed, so the host controller (`CUSBHCIDevice`) used for Ethernet is untouched. `Process()` also runs in the degraded and fatal loops, so the error message can be read over USB.

### D5 — USB identity
Vendor ID `0x1209` (pid.codes, for open-source hardware/firmware). Until a product ID is assigned, the pid.codes test ID `0x0001` is used (allowed for testing, not for distribution); the constants are in one place (`USB_LOG_VENDOR_ID`, `USB_LOG_PRODUCT_ID` in `logtee.h`).

## Risks / Trade-offs

- [No board here to test USB device mode; QEMU's `raspi*` machines have no USB device-mode emulation] → ring logic in host tests; firmware build for all targets; hardware tests added to `verify-ir-blaster-hardware` (Zero W and Pi 4 B with a Mac).
- [Pi 4 B powered only by the Mac's USB-C port (≈ 15 W)] → enough without USB peripherals; README recommends a powered hub if the Pi resets under load.
- [Self-powered board plus USB cable: Circle warns that both supplies must have the same voltage] → README: use only the USB cable to the Mac for power while logging over USB, or a data-only connection.
- [Interrupt-context logging contends with the main task on the ring spin lock] → bounded copy (log lines ≤ 512 bytes), same pattern as the IR assembler.
- [Using a test PID in a published release] → open question below; task to request a PID before the release.

## Open Questions

- pid.codes product ID: request one via a pull request to `pid-codes/pidcodes.github.com` (needs the maintainer, public). Until then the test PID `0x1209:0x0001` is used.
