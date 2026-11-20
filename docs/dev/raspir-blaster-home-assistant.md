# Bare-Metal IR Blaster & Receiver for Home Assistant (Raspberry Pi & Circle)

This document summarizes all responses and code components from the entire session—from the general system architecture and specific adaptation for the Raspberry Pi Zero W to the Makefile, JSON parser, MQTT Discovery, and auto-reconnect.

---

## Part 1: General System Architecture & Fundamentals

To build a bare-metal IR blaster and receiver for Home Assistant using **Circle** (the C++ bare-metal programming environment for Raspberry Pi), you will run your firmware directly on the Pi's CPU without Linux.

While your prompt mentions "mqtt via infrared," standard Home Assistant integrations communicate with the Pi via **MQTT over Wi-Fi/Ethernet**, and the Pi translates those MQTT payloads into **infrared signals** (and vice versa).

### 1. Hardware Requirements
* **Raspberry Pi:** Pi Zero W with the BCM2835 SoC (Circle supports it, including onboard Wi-Fi).

**IR receiver TSOP38238**

| TSOP Pin | RasPi GPIO Header Pin |
| -------- | --------------------- |
| Signal   | 16 (GPIO23, edge IRQ) |
| GND.     | 9                     |
| VCC.     | 1 (3,3V)              |

**IR transmitter AZ-Delivery KY-005**

| KY-005 Pin | RasPi GPIO Header Pin |
| ---------- | --------------------- |
| Signal.    | 12 (GPIO18, PWM0)     |
| GND        | 6                     |
| VCC        | 2 (5V)                |

### 2. Circle Framework Setup
Circle provides built-in libraries for networking, GPIO, hardware timers, and a native MQTT client.

1. **Clone Circle:** Clone the `rsta2/circle` repository on your development machine.
2. **Build Dependencies:** Build the core Circle libraries along with the networking (`addon/wlan`, `lib/net`) and MQTT (`lib/net/mqttclient.cpp`) modules.
3. **Wi-Fi Firmware:** If using Wi-Fi, copy the Raspberry Pi wireless firmware files (`brcmfmac43430-sdio.*` or similar depending on your Pi model) onto your SD card's boot partition alongside a `wpa_supplicant.conf` file.

### 3. Core Software Architecture
Because Circle is bare-metal, you have deterministic, microsecond-level control over GPIO pins—ideal for IR protocols.

#### A. Networking & MQTT (`CMQTTClient`)
* Initialize `CNetSubSystem` and `CWPASupplicant` (for Wi-Fi) in your `CKernel::Run()` loop.
* Instantiate Circle's `CMQTTClient` to connect to your Home Assistant Mosquitto broker.
* **Subscribe** to a command topic (e.g., `home/ir_blaster/send`).
* **Publish** decoded IR signals to a state topic (e.g., `home/ir_blaster/received`).

#### B. IR Transmission (38kHz Carrier Modulation)
IR remotes modulate data onto a 38kHz carrier wave (one cycle ≈ 26.3 μs).
* **Hardware PWM (Recommended):** Use Circle's `CPWMOutput` class on the PWM-capable pin GPIO18 (header pin 12, PWM0) configured to 38,000 Hz with a 33% or 50% duty cycle. Toggle the PWM output on and off using `CTimer::SimpleusDelay()` to match the protocol's mark/space timings (e.g., NEC or Sony protocols).
* **Bit-Banging (Alternative):** Toggle a standard GPIO pin high for 13 μs and low for 13 μs in a tight loop using `CTimer::GetClockTicks()`. Because there is no OS scheduler interrupting your thread, bare-metal bit-banging is remarkably accurate.

#### C. IR Receiving (Interrupt-Driven)
* Configure your receiver pin using `CGPIOPin` with `GPIOModeInputPullUp`.
* Attach an interrupt handler via `CGPIOPin::ConnectInterrupt()` triggered on `GPIOInterruptOnFall` and `GPIOInterruptOnRise`.
* Inside the Interrupt Service Routine (ISR), record the current microsecond timestamp using `CTimer::GetClockTicks()`.
* Calculate the delta (Δt) between edges to reconstruct the pulse-distance array, then decode it (e.g., matching NEC's 9ms leading pulse + 4.5ms space) in your main loop before publishing to MQTT.

### 4. Home Assistant Integration
Once your Circle firmware publishes and subscribes to MQTT, you can integrate it into Home Assistant via `configuration.yaml` or **MQTT Discovery**.

* **Triggering an IR blast from HA:** Create an MQTT button or script that publishes a JSON payload (e.g., `{"protocol": "NEC", "address": "0x04", "command": "0x08"}`) to `home/ir_blaster/send`.
* **Receiving IR commands in HA:** Use an MQTT Sensor or MQTT Device Trigger listening to `home/ir_blaster/received` to trigger automations when you press a button on a physical remote.

### Sample Code Skeleton (`kernel.cpp`)

```cpp
#include <circle/koptions.h>
#include <circle/devicenameservice.h>
#include <circle/net/netsubsystem.h>
#include <circle/net/mqttclient.h>
#include <circle/gpiopin.h>
#include <circle/timer.h>

// Example: Sending a raw 38kHz mark (burst) for 'usec' microseconds
void SendIRMark(CGPIOPin &IRLedPin, unsigned usec) {
    unsigned startTicks = CTimer::GetClockTicks();
    // 1 MHz clock ticks in Circle (1 tick = 1 microsecond)
    while ((CTimer::GetClockTicks() - startTicks) < usec) {
        IRLedPin.Write(HIGH);
        CTimer::SimpleusDelay(13); // ~38kHz half-period
        IRLedPin.Write(LOW);
        CTimer::SimpleusDelay(13);
    }
}

void SendIRSpace(unsigned usec) {
    CTimer::SimpleusDelay(usec);
}
```

---

## Part 2: Tailored Solution for Raspberry Pi Zero W

The Raspberry Pi Zero W is based on the **BCM2835** (single-core ARM1176JZF-S, ARMv6, the same SoC as the Pi 1 and Pi Zero) and uses the CYW43438 Wi-Fi chip. In Circle, you therefore compile the project for the **`RASPPI=1`** architecture. This SoC only supports 32-bit mode (`AARCH=32`, toolchain prefix `arm-none-eabi-`).

### 1. Preparing the SD Card (Zero W & Wi-Fi)
For Circle to initialize the Zero W's onboard Wi-Fi, the driver files from the Circle repository (`addon/wlan/firmware`) must be placed on the FAT32 boot partition of the SD card.

* **Boot files:** `bootcode.bin`, `start.elf`, `fixup.dat`, and your compiled kernel image (`kernel.img`).
* **Wi-Fi firmware:** Copy `brcmfmac43430-sdio.bin` and `brcmfmac43430-sdio.txt` into the root directory (or the `firmware/` subfolder) of the SD card.
* **Wi-Fi credentials (`wpa_supplicant.conf`):** Create this file on the SD card as well:
  ```text
  country=DE
  network={
      ssid="YOUR_WIFI_SSID"
      psk="YOUR_WIFI_PASSWORD"
  }
  ```

### 2. Hardware Pinout for the Pi Zero W
For optimal signal quality without CPU overhead from bit-banging, using the Zero W's hardware PWM channel is recommended.

* **IR Transmitter (Hardware PWM):** Connect **GPIO 18** (Pin 12) via a 1kΩ resistor to the base of an NPN transistor (e.g., 2N2222) to drive the 940nm IR LED with 5V (via a 100Ω current-limiting resistor).
* **IR Receiver (Interrupt):** Connect **GPIO 23** (Pin 16) directly to the data pin of a 38kHz demodulator (e.g., TSOP38238), powered by 3.3V (Pin 1) and GND (Pin 9).

### 3. Architecture for All IR Protocols & Air Conditioners
Since you want to support both standard protocols (NEC, Sony, RC5/RC6) and long air conditioner frames (often 100 to 300+ edge transitions), a **hybrid raw-timing approach** is the most reliable.

* **Why raw timings?** Air conditioner remotes send the entire state (temperature, mode, fan speed) as a long bit pattern with every button press. If your Pi receives and transmits all signals as microsecond arrays (`uint16_t raw[]`), you cover **100% of all protocols** without having to write a separate parser for every air conditioner brand.
* **Bi-phase protocols (RC5/RC6):** These use Manchester encoding instead of pulse-distance modulation. By recording the exact edge timings (`Mark` and `Space` in µs), RC5 (36 kHz) and RC6 are captured and replayed just as losslessly as NEC or Sony (40 kHz).
* **Buffer size in Circle:** Size your receive ring buffer to at least **512 entries** (`uint16_t m_RawBuffer`) so long Daikin, Mitsubishi, or LG air conditioner telegrams are not truncated.

### 4. Circle C++ Starter Template (`kernel.cpp`)

This skeleton initializes the onboard Wi-Fi, connects to your MQTT broker, captures IR signals via hardware interrupt, and transmits received JSON arrays via hardware PWM.

```cpp
#include <circle/actled.h>
#include <circle/koptions.h>
#include <circle/devicenameservice.h>
#include <circle/screen.h>
#include <circle/serial.h>
#include <circle/exceptionhandler.h>
#include <circle/interrupt.h>
#include <circle/timer.h>
#include <circle/gpiopin.h>
#include <circle/pwmoutput.h>
#include <circle/net/netsubsystem.h>
#include <circle/net/mqttclient.h>
#include <wlan/bcm4343.h>
#include <wlan/hostap/wpa_supplicant/wpasupplicant.h>
#include <circle/util.h>

#define MAX_IR_TIMINGS 512
#define IR_TIMEOUT_US  15000 // 15ms pause ends an air conditioner frame

// MQTT Configuration
static const u8 MQTT_BROKER_IP[] = {192, 168, 1, 100}; // IP of your Home Assistant broker
#define MQTT_PORT        1883
#define TOPIC_SEND       "home/ir_blaster/send"
#define TOPIC_RECEIVED   "home/ir_blaster/received"

class CKernel {
public:
    CKernel(void)
    : m_Screen(m_Options.GetWidth(), m_Options.GetHeight()),
      m_Timer(&m_Interrupt),
      m_WLAN("SD:/firmware/"),
      m_Net(0, 0, 0, 0, "ir-blaster", NetDeviceTypeWLAN),
      m_WPASupplicant("SD:/wpa_supplicant.conf"),
      m_IRRecvPin(23, GPIOModeInputPullUp, &m_Interrupt),
      m_PWMOutput(PWMClockRate, 100, TRUE), // Configured for 38kHz carrier frequency
      m_MQTTClient(&m_Net)
    {
    }

    boolean Initialize(void) {
        if (!m_Screen.Initialize()) return FALSE;
        if (!m_Interrupt.Initialize()) return FALSE;
        if (!m_Timer.Initialize()) return FALSE;
        if (!m_WLAN.Initialize()) return FALSE;
        if (!m_Net.Initialize(FALSE)) return FALSE;
        if (!m_WPASupplicant.Initialize()) return FALSE;

        // Prepare hardware PWM on GPIO 18 (38 kHz)
        m_PWMOutput.Start();

        // Register interrupt for IR receiver on GPIO 23
        m_IRRecvPin.ConnectInterrupt(IRInterruptHandler, this);
        m_IRRecvPin.EnableInterrupt(GPIOInterruptOnBothEdges);

        return TRUE;
    }

    void Run(void) {
        // Wait for Wi-Fi connection & DHCP
        while (!m_Net.IsRunning()) {
            m_Timer.MsDelay(100);
        }

        // Establish MQTT connection
        CIPAddress BrokerIP(MQTT_BROKER_IP);
        m_MQTTClient.Connect(BrokerIP, MQTT_PORT, "pi_zero_ir");
        m_MQTTClient.Subscribe(TOPIC_SEND);

        while (TRUE) {
            // Check if an IR frame has been completely received (timeout after last edge)
            if (m_PulseCount > 0 && (CTimer::GetClockTicks() - m_LastEdgeTick) > IR_TIMEOUT_US) {
                m_IRRecvPin.DisableInterrupt();
                PublishReceivedIR();
                m_PulseCount = 0;
                m_IRRecvPin.EnableInterrupt(GPIOInterruptOnBothEdges);
            }

            m_Timer.MsDelay(5);
        }
    }

private:
    static void IRInterruptHandler(void *pParam) {
        CKernel *pThis = (CKernel *)pParam;
        unsigned now = CTimer::GetClockTicks();

        if (pThis->m_PulseCount == 0) {
            pThis->m_LastEdgeTick = now;
            pThis->m_PulseCount = 1;
            return;
        }

        unsigned delta = now - pThis->m_LastEdgeTick;
        pThis->m_LastEdgeTick = now;

        if (pThis->m_PulseCount < MAX_IR_TIMINGS) {
            pThis->m_RawBuffer[pThis->m_PulseCount - 1] = (u16)delta;
            pThis->m_PulseCount++;
        }
    }

    void PublishReceivedIR(void) {
        // Formats the microsecond values as a JSON array: {"freq":38000,"raw":[9000,4500,560,...]}
        char payload[4096];
        int pos = snprintf(payload, sizeof(payload), "{"freq":38000,"raw":[");
        for (unsigned i = 0; i < m_PulseCount - 1; i++) {
            pos += snprintf(payload + pos, sizeof(payload) - pos, "%u%s",
                            m_RawBuffer[i], (i < m_PulseCount - 2) ? "," : "");
        }
        snprintf(payload + pos, sizeof(payload) - pos, "]}");

        m_MQTTClient.Publish(TOPIC_RECEIVED, payload);
    }

    void SendRawIR(const u16 *timings, unsigned count, unsigned freqHz = 38000) {
        // Dynamically adjusts PWM frequency (e.g., 38kHz for NEC/AC, 40kHz for Sony, 36kHz for RC5)
        for (unsigned i = 0; i < count; i++) {
            if (i % 2 == 0) {
                // Even indices = IR burst (Mark) -> Turn PWM on (33% duty cycle)
                m_PWMOutput.Write(PWM_CHANNEL1, 33);
            } else {
                // Odd indices = Pause (Space) -> Turn PWM off
                m_PWMOutput.Write(PWM_CHANNEL1, 0);
            }
            CTimer::SimpleusDelay(timings[i]);
        }
        m_PWMOutput.Write(PWM_CHANNEL1, 0);
    }

    CKernelOptions m_Options;
    CScreenDevice m_Screen;
    CExceptionHandler m_ExceptionHandler;
    CInterruptSystem m_Interrupt;
    CTimer m_Timer;
    CBcm4343Device m_WLAN;
    CNetSubSystem m_Net;
    CWPASupplicant m_WPASupplicant;
    CGPIOPinInterrupt m_IRRecvPin;
    CPWMOutput m_PWMOutput;
    CMQTTClient m_MQTTClient;

    volatile unsigned m_PulseCount = 0;
    volatile unsigned m_LastEdgeTick = 0;
    u16 m_RawBuffer[MAX_IR_TIMINGS];
};
```

*(Note: In Circle's `CMQTTClient`, you override the virtual method `OnMessage(const char *pTopic, const u8 *pPayload, size_t nLength)`, parse the JSON array there, and pass the values to `SendRawIR()`).*

### 5. Integration into Home Assistant (`configuration.yaml`)

With this configuration, you can receive, record, and re-transmit signals at the push of a button in Home Assistant.

```yaml
mqtt:
  # 1. Receiver sensor for learning new remote controls
  sensor:
    - name: "IR Blaster Received"
      state_topic: "home/ir_blaster/received"
      value_template: "{{ value_json.raw | length }} Pulses"
      json_attributes_topic: "home/ir_blaster/received"

  # 2. Example button for sending a stored signal (e.g., TV Power or Air Conditioner)
  button:
    - name: "Living Room AC On"
      command_topic: "home/ir_blaster/send"
      payload_press: >
        {
          "freq": 38000,
          "raw": [9000, 4500, 560, 560, 560, 1690]
        }
```

#### Workflow for Learning a Remote Control:
1. Press a button on your TV or air conditioner remote pointed at the Pi Zero W.
2. In Home Assistant, open **Developer Tools → States** and search for `sensor.ir_blaster_received`.
3. Copy the entire array from the `raw` attribute directly into the `payload_press` block of your MQTT button or an automation.

---

## Part 3: `Makefile` Configuration & MQTT Callback with JSON Parser

### 1. The `Makefile` for the Raspberry Pi Zero W

Ideally, place your project as a subfolder inside the Circle repository (e.g., `circle/app/ir-blaster/`). For the Wi-Fi subsystem (`CBcm4343Device` and `CWPASupplicant`), SD card access (`FatFs`), and MQTT to link cleanly, the `Makefile` must include all corresponding `.a` libraries.

Create the file `circle/app/ir-blaster/Makefile`:

```makefile
# Adjust path to the Circle root directory if the project is located outside
CIRCLEHOME = ../..

# Architecture settings for Raspberry Pi Zero W
RASPPI = 1
AARCH = 32
PREFIX = arm-none-eabi-

OBJS = main.o kernel.o

# Required Circle libraries (linking order matters!)
LIBS = $(CIRCLEHOME)/addon/wlan/libwlan.a        $(CIRCLEHOME)/addon/wlan/hostap/wpa_supplicant/libwpa_supplicant.a        $(CIRCLEHOME)/addon/SDCard/libsdcard.a        $(CIRCLEHOME)/addon/fatfs/libfatfs.a        $(CIRCLEHOME)/lib/usb/libusb.a        $(CIRCLEHOME)/lib/input/libinput.a        $(CIRCLEHOME)/lib/fs/libfs.a        $(CIRCLEHOME)/lib/net/libnet.a        $(CIRCLEHOME)/lib/sched/libsched.a        $(CIRCLEHOME)/lib/libcircle.a

include $(CIRCLEHOME)/Rules.mk

# Add include paths for Wi-Fi and SD card
CFLAGS += -I $(CIRCLEHOME)/addon/wlan -I $(CIRCLEHOME)/addon/SDCard -I $(CIRCLEHOME)/addon/fatfs
```

#### Build Sequence in the Terminal:
Before building your project, the Circle core and addon libraries must be compiled once for `RASPPI=1`:
```bash
# 1. Create Config.mk in the Circle root directory
echo "RASPPI = 1" > Config.mk
echo "AARCH = 32" >> Config.mk
echo "PREFIX = arm-none-eabi-" >> Config.mk

# 2. Build base libraries and addons
./makeall
cd addon/SDCard && make && cd ../..
cd addon/fatfs && make && cd ../..
cd addon/wlan && make && cd ../..

# 3. Build your IR blaster project
cd app/ir-blaster && make
```
The compiled kernel image will then be located as **`kernel.img`** in your project folder.

### 2. MQTT Callback & JSON Parser (`CIRMQTTClient`)

In Circle, you inherit from `CMQTTClient` and override the virtual method `OnMessage()`. Since a full JSON parser creates unnecessary overhead on bare-metal, a fast, safe **zero-allocation parser** in a few lines of C++ is sufficient for the format `{"freq":38000,"raw":[9000,4500,...]}`.

Create a custom client class (e.g., `irmqttclient.h` or directly before `CKernel` in `kernel.h`):

```cpp
#include <circle/net/mqttclient.h>
#include <circle/util.h>
#include <stdlib.h>

// Function pointer type for passing data to CKernel::SendRawIR
typedef void (*TIRSendCallback)(const u16 *pTimings, unsigned nCount, unsigned nFreqHz, void *pParam);

class CIRMQTTClient : public CMQTTClient {
public:
    CIRMQTTClient(CNetSubSystem *pNetSubSystem, TIRSendCallback pCallback, void *pParam)
    : CMQTTClient(pNetSubSystem),
      m_pCallback(pCallback),
      m_pParam(pParam)
    {
    }

    // Automatically called by Circle when an MQTT message arrives
    void OnMessage(const char *pTopic, const u8 *pPayload, size_t nLength) override {
        if (strcmp(pTopic, "home/ir_blaster/send") != 0 || nLength == 0) {
            return;
        }

        // Safely null-terminate payload (max. 4 KB for long air conditioner frames)
        char buf[4096];
        size_t copyLen = (nLength < sizeof(buf) - 1) ? nLength : sizeof(buf) - 1;
        memcpy(buf, pPayload, copyLen);
        buf[copyLen] = ' ';

        // 1. Read optional "freq" field (default: 38000 Hz)
        unsigned freqHz = 38000;
        const char *pFreq = strstr(buf, ""freq"");
        if (pFreq != nullptr) {
            const char *pColon = strchr(pFreq, ':');
            if (pColon != nullptr) {
                unsigned parsedFreq = (unsigned)strtoul(pColon + 1, nullptr, 10);
                if (parsedFreq >= 20000 && parsedFreq <= 60000) {
                    freqHz = parsedFreq;
                }
            }
        }

        // 2. Locate the "raw" array and parse it into u16 values
        const char *pRaw = strstr(buf, ""raw"");
        if (pRaw == nullptr) return;

        const char *pBracketStart = strchr(pRaw, '[');
        const char *pBracketEnd = strchr(pRaw, ']');
        if (pBracketStart == nullptr || pBracketEnd == nullptr || pBracketEnd <= pBracketStart) {
            return;
        }

        u16 timings[512];
        unsigned count = 0;
        const char *pCursor = pBracketStart + 1;

        while (pCursor < pBracketEnd && count < 512) {
            // Skip whitespace and commas
            while (pCursor < pBracketEnd && (*pCursor == ' ' || *pCursor == ',' || *pCursor == '
' || *pCursor == '
')) {
                pCursor++;
            }
            if (pCursor >= pBracketEnd) break;

            char *pEndNum = nullptr;
            unsigned long val = strtoul(pCursor, &pEndNum, 10);
            if (pEndNum == pCursor) break; // No valid numeric value left

            timings[count++] = (u16)(val > 65535 ? 65535 : val);
            pCursor = pEndNum;
        }

        // 3. Trigger callback to transmit via hardware PWM
        if (count > 0 && m_pCallback != nullptr) {
            m_pCallback(timings, count, freqHz, m_pParam);
        }
    }

private:
    TIRSendCallback m_pCallback;
    void *m_pParam;
};
```

### 3. Integration into `CKernel` & Dynamic Carrier Frequency

In your `CKernel` class, replace `CMQTTClient` with `CIRMQTTClient` and adjust the PWM frequency dynamically before transmitting (important because Sony uses **40 kHz**, NEC/air conditioners use **38 kHz**, and Philips RC5 uses **36 kHz**):

```cpp
// In the CKernel constructor:
m_MQTTClient(&m_Net, OnIRSendRequest, this)

// Static wrapper function in CKernel:
static void OnIRSendRequest(const u16 *pTimings, unsigned nCount, unsigned nFreqHz, void *pParam) {
    CKernel *pThis = (CKernel *)pParam;
    pThis->SendRawIR(pTimings, nCount, nFreqHz);
}

// Updated SendRawIR method in CKernel:
void SendRawIR(const u16 *timings, unsigned count, unsigned freqHz) {
    // Disable receiver interrupt during transmission to avoid recording our own signals
    m_IRRecvPin.DisableInterrupt();

    // Calculate PWM divider for the desired carrier frequency (base clock in Circle: 100 MHz / Range 100)
    // At Range = 100, ClockRate = freqHz * 100
    m_PWMOutput.Stop();
    m_PWMOutput.SetClockRate(freqHz * 100);
    m_PWMOutput.Start();

    for (unsigned i = 0; i < count; i++) {
        if (i % 2 == 0) {
            m_PWMOutput.Write(PWM_CHANNEL1, 33); // 33% duty cycle (protects the IR LED at high current)
        } else {
            m_PWMOutput.Write(PWM_CHANNEL1, 0);
        }
        CTimer::SimpleusDelay(timings[i]);
    }

    m_PWMOutput.Write(PWM_CHANNEL1, 0);

    // Re-enable receiver
    m_PulseCount = 0;
    m_IRRecvPin.EnableInterrupt(GPIOInterruptOnBothEdges);
}
```

#### Important Note on the SD Card (`EMMCDevice`)
Since `CBcm4343Device` and `CWPASupplicant` read the firmware and configuration files from `SD:/`, you must also initialize the SD card device (`CEMMCDevice m_EMMC`) and the filesystem (`FATFS m_FileSystem` via `f_mount(&m_FileSystem, "SD:", 1)`) in `CKernel` before `m_WLAN.Initialize()`. Check the example `circle/addon/wlan/sample/kernel.cpp`—it contains the exact 5 lines needed for mounting.

---

## Part 4: Home Assistant MQTT Discovery & Auto-Reconnect

### 1. Home Assistant MQTT Discovery & Availability

With **MQTT Discovery**, Home Assistant automatically recognizes your Raspberry Pi Zero W as a device ("Pi Zero IR Blaster") under **Settings → Devices & Services → MQTT**—without any manual entries in `configuration.yaml`.

So that Home Assistant knows whether the Pi is reachable, we use an **Availability Topic** (`home/ir_blaster/status`) with **Last Will and Testament (LWT)**:
* Upon connecting, the Pi sends `online` (with `retain = true`).
* If the connection drops, the MQTT broker automatically publishes the LWT payload `offline` registered during connect.

#### C++ Function for Discovery (`PublishDiscovery()`)
Call this method once immediately after a successful `m_MQTTClient.Connect()`:

```cpp
#define TOPIC_STATUS          "home/ir_blaster/status"
#define TOPIC_DISCOVERY_SENS  "homeassistant/sensor/pi_zero_ir/received/config"

void PublishDiscovery(void) {
    // 1. Publish online status with retain flag
    m_MQTTClient.Publish(TOPIC_STATUS, "online", TRUE);

    // 2. Discovery payload for the receiver sensor (including device metadata)
    const char *discoveryPayload =
        "{"
          ""name":"IR Receiver","
          ""unique_id":"pi_zero_ir_recv","
          ""state_topic":"home/ir_blaster/received","
          ""value_template":"{{ value_json.raw | length }} Pulses","
          ""json_attributes_topic":"home/ir_blaster/received","
          ""availability_topic":"home/ir_blaster/status","
          ""icon":"mdi:remote","
          ""device":{"
            ""identifiers":["pi_zero_ir_blaster"],"
            ""name":"Pi Zero IR Blaster","
            ""model":"Raspberry Pi Zero W (Circle Bare-Metal)","
            ""manufacturer":"Custom DIY""
          "}"
        "}";

    // Send with retain = TRUE so HA recognizes the device immediately after a restart
    m_MQTTClient.Publish(TOPIC_DISCOVERY_SENS, discoveryPayload, TRUE);
}
```
*(Tip: Once you have recorded remote control codes, you can add additional `homeassistant/button/pi_zero_ir/<button_name>/config` payloads following the same pattern, pointing to `"command_topic":"home/ir_blaster/send"`).*

### 2. Auto-Reconnect for Wi-Fi and MQTT

Since there is no Linux network manager running in the background on a bare-metal system, your main loop (`CKernel::Run()`) periodically checks the connection status of Wi-Fi and MQTT and automatically re-establishes the connection in case of an interruption (e.g., router reboot or Home Assistant update).

In Circle's `CMQTTClient`, you can query `IsConnected()` and override the virtual method `OnDisconnect()`.

#### Extension in `CIRMQTTClient`
Add a status flag in your `CIRMQTTClient` class that is set when the connection is lost:

```cpp
class CIRMQTTClient : public CMQTTClient {
public:
    CIRMQTTClient(CNetSubSystem *pNetSubSystem, TIRSendCallback pCallback, void *pParam)
    : CMQTTClient(pNetSubSystem),
      m_pCallback(pCallback),
      m_pParam(pParam),
      m_bConnected(FALSE)
    {
    }

    boolean IsClientConnected(void) const {
        return m_bConnected;
    }

    void SetConnected(boolean bState) {
        m_bConnected = bState;
    }

    void OnDisconnect(void) override {
        m_bConnected = FALSE;
    }

    // ... OnMessage() remains unchanged as before ...

private:
    TIRSendCallback m_pCallback;
    void *m_pParam;
    volatile boolean m_bConnected;
};
```

#### State Machine in `CKernel::Run()`
Replace your `Run()` loop with this non-blocking reconnect logic featuring a backoff timer (so the Pi does not flood the broker with requests during an outage):

```cpp
void Run(void) {
    CIPAddress BrokerIP(MQTT_BROKER_IP);
    unsigned lastReconnectAttempt = 0;
    const unsigned RECONNECT_INTERVAL_MS = 5000; // Retry every 5 seconds

    while (TRUE) {
        unsigned nowMs = m_Timer.GetClockTicks() / 1000;

        // 1. Check if Wi-Fi & DHCP are active (CWPASupplicant reconnects Wi-Fi in the background)
        if (m_Net.IsRunning()) {
            // 2. Check if MQTT is connected
            if (!m_MQTTClient.IsClientConnected()) {
                if ((nowMs - lastReconnectAttempt) >= RECONNECT_INTERVAL_MS) {
                    lastReconnectAttempt = nowMs;

                    // Connect with Last Will & Testament (Topic: TOPIC_STATUS, Payload: "offline", Retain: TRUE)
                    if (m_MQTTClient.Connect(BrokerIP, MQTT_PORT, "pi_zero_ir",
                                             nullptr, nullptr, // Optional: MQTT User & Password
                                             TOPIC_STATUS, "offline", TRUE)) {
                        m_MQTTClient.SetConnected(TRUE);
                        m_MQTTClient.Subscribe(TOPIC_SEND);
                        PublishDiscovery();
                    }
                }
            }
        } else {
            // Wi-Fi connection lost -> reset MQTT status
            m_MQTTClient.SetConnected(FALSE);
        }

        // 3. Evaluate and transmit received IR frame
        if (m_PulseCount > 0 && (CTimer::GetClockTicks() - m_LastEdgeTick) > IR_TIMEOUT_US) {
            m_IRRecvPin.DisableInterrupt();

            // Only transmit if an active MQTT connection exists
            if (m_MQTTClient.IsClientConnected()) {
                PublishReceivedIR();
            }

            m_PulseCount = 0;
            m_IRRecvPin.EnableInterrupt(GPIOInterruptOnBothEdges);
        }

        m_Timer.MsDelay(5);
    }
}
```

### Practical Tip for Continuous Operation
* **MQTT Authentication:** If your Mosquitto broker in Home Assistant requires a username and password (standard setup), simply pass them instead of `nullptr, nullptr` to `m_MQTTClient.Connect(..., "mqtt_user", "mqtt_password", ...)`.
* **Hardware Watchdog (Optional):** Circle provides the `CBcmWatchdog` class (`#include <circle/bcmwatchdog.h>`). If you enable it with a timeout of e.g. 10 seconds and periodically reset it inside the `while(TRUE)` loop (`m_Watchdog.Start(10)`), the Pi Zero W will automatically reboot itself if an unexpected hang occurs in the Wi-Fi driver.
