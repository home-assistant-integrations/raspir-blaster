## ADDED Requirements

### Requirement: Edge capture on GPIO23
The firmware SHALL configure GPIO23 (header pin 16), connected to the TSOP38238 output, as an input with pull-up. It SHALL record the time of every rising and falling edge with 1 µs resolution using an interrupt. The first falling edge of a frame marks the start of the first mark (the TSOP output is active-low).

#### Scenario: Edges are timed
- **WHEN** an IR signal with a 9000 µs mark followed by a 4500 µs space is received
- **THEN** the captured frame starts with values within ±100 µs of 9000 and 4500

### Requirement: Frame segmentation by silence
A frame SHALL be considered complete when no edge has occurred for `ir_frame_gap_ms` milliseconds (default 15) after the last edge. The trailing silence SHALL NOT be part of the frame, so a complete frame ends with a mark.

#### Scenario: Single frame
- **WHEN** a remote sends one frame followed by 100 ms of silence
- **THEN** exactly one frame is produced, and its last element is a mark

#### Scenario: Repeat frames
- **WHEN** a remote sends a frame, then 40 ms of silence, then a repeat code
- **THEN** two separate frames are produced, and neither is lost

#### Scenario: Configured gap
- **WHEN** `ir_frame_gap_ms=30` is configured and a signal contains a 20 ms internal pause
- **THEN** the signal is captured as one frame including the 20 ms space

### Requirement: Frame limits and noise filtering
A frame SHALL hold at most 512 timings and last at most 2 s in total. A frame exceeding either limit SHALL be discarded completely and an overflow logged. This keeps every published frame valid for retransmission. A frame with fewer than 4 timings SHALL be treated as noise and discarded silently.

#### Scenario: Overflow
- **WHEN** a signal produces more than 512 timings before the silence gap
- **THEN** no frame is published and an overflow is logged

#### Scenario: Noise burst
- **WHEN** a single spurious pulse produces 1 timing
- **THEN** nothing is published

### Requirement: Publish received frames
Each complete frame SHALL be published to `<topic_base>/received` in the canonical IR message format with `freq` 38000 (the demodulating receiver cannot measure the carrier), QoS 0, not retained. Frames completed while MQTT is disconnected SHALL be discarded, not queued.

#### Scenario: Remote button press
- **WHEN** a button on a physical remote is pressed while MQTT is connected
- **THEN** a message `{"freq":38000,"raw":[...]}` is published to `<topic_base>/received` within 100 ms after the frame ends

#### Scenario: Disconnected
- **WHEN** a frame completes while MQTT is disconnected
- **THEN** the frame is discarded and is not published after reconnecting

### Requirement: Capture continues during publishing
Capturing the next frame SHALL NOT be blocked while the previous frame is being published.

#### Scenario: Back-to-back frames
- **WHEN** a second frame starts while the first is being serialized and published
- **THEN** both frames are published completely and in order
