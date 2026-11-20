## ADDED Requirements

### Requirement: Transmit valid messages from MQTT
For every valid IR message received on `<topic_base>/send`, the firmware SHALL transmit the frame once via the IR LED connected to GPIO18 (header pin 12).

#### Scenario: Send command from Home Assistant
- **WHEN** Home Assistant publishes `{"freq":38000,"raw":[9000,4500,560,560,560]}` to `<topic_base>/send`
- **THEN** the IR LED emits one frame with marks of 9000, 560 and 560 µs and spaces of 4500 and 560 µs

#### Scenario: Invalid message
- **WHEN** an invalid message is received
- **THEN** nothing is transmitted

#### Scenario: Retained message
- **WHEN** a message on `<topic_base>/send` is delivered with the retain flag set (e.g. after reconnecting)
- **THEN** it is ignored, nothing is transmitted, and the event is logged

### Requirement: Hardware PWM carrier
During marks, the firmware SHALL generate the carrier on GPIO18 with the BCM2835 hardware PWM (PWM0) at the message's `freq`, with a duty cycle of 33 % ± 3 %. The carrier frequency SHALL be within ±1 % of the requested value. During spaces and when idle, GPIO18 SHALL be held low (LED off).

#### Scenario: Carrier frequency
- **WHEN** a message with `freq` 36000, 38000 or 40000 is transmitted
- **THEN** the measured carrier frequency during marks is within ±1 % of the requested value

#### Scenario: Idle state
- **WHEN** no transmission is in progress
- **THEN** GPIO18 is low

### Requirement: Timing accuracy
Each mark and space SHALL be timed against absolute deadlines measured from the start of the frame, so errors do not accumulate. Every edge SHALL occur within ±50 µs of its nominal time.

#### Scenario: Long air-conditioner frame
- **WHEN** a frame of 300 timings with a total duration of 400 ms is transmitted
- **THEN** the last edge occurs within ±50 µs of its nominal time

### Requirement: Receiver suppressed during transmission
The firmware SHALL ignore edges on the IR receiver while transmitting and for 5 ms after the last mark. It SHALL discard any partially captured frame when a transmission starts.

#### Scenario: No self-capture
- **WHEN** the device transmits a frame while its receiver faces the LED
- **THEN** no frame is published to `<topic_base>/received` as a result of the transmission

### Requirement: Sequential transmission queue
Transmissions SHALL never overlap. Valid messages SHALL be queued in arrival order (queue depth 4) and transmitted one after another. A message arriving while the queue is full SHALL be dropped and the drop logged.

#### Scenario: Burst of commands
- **WHEN** three valid messages arrive within 10 ms
- **THEN** three frames are transmitted, one after another, in arrival order

#### Scenario: Queue full
- **WHEN** a fifth message arrives while four are queued
- **THEN** that message is dropped, the drop is logged, and the four queued frames are still transmitted
