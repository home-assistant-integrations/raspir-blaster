# ir-message-format Specification

## Purpose
The JSON payload format for raw IR frames exchanged over MQTT (carrier frequency and mark/space timings) and the limits every message is validated against.
## Requirements
### Requirement: Raw IR frame message schema
An IR frame SHALL be represented as a UTF-8 JSON object with:
- `raw` (required): an array of positive integers giving alternating mark and space durations in microseconds, starting with a mark.
- `freq` (optional): the carrier frequency in Hz; default `38000`.

The same schema SHALL be used for messages received on `<topic_base>/send` and published on `<topic_base>/received`.

#### Scenario: Minimal message
- **WHEN** the payload is `{"raw":[9000,4500,560]}`
- **THEN** it is accepted with carrier 38000 Hz and timings 9000, 4500, 560

#### Scenario: Message with frequency
- **WHEN** the payload is `{"freq":36000,"raw":[889,889,1778]}`
- **THEN** it is accepted with carrier 36000 Hz

### Requirement: Tolerant syntax
The parser SHALL accept keys in any order, arbitrary JSON whitespace (space, tab, CR, LF) between tokens, and unknown keys with scalar values (string, number, boolean, null), which it ignores.

#### Scenario: Reordered keys and whitespace
- **WHEN** the payload is `{ "raw" : [ 560 ,\n 560 ] , "freq" : 40000 }`
- **THEN** it is accepted with carrier 40000 Hz and timings 560, 560

#### Scenario: Unknown key
- **WHEN** the payload contains `"name":"tv_power"` in addition to `raw`
- **THEN** the unknown key is ignored and the message is accepted

### Requirement: Validation limits
The parser SHALL reject a message, without transmitting anything, if any of these holds:
- the payload is longer than 4096 bytes
- `raw` is missing, is not an array, is empty, or has more than 512 elements
- an element of `raw` is not an integer in the range 1–65535
- the sum of all `raw` elements exceeds 2,000,000 µs (2 s)
- `freq` is present but is not an integer in the range 20000–60000
- the payload is not a syntactically valid JSON object

Values SHALL NOT be clamped or silently replaced.

#### Scenario: Too many timings
- **WHEN** `raw` contains 513 elements
- **THEN** the message is rejected

#### Scenario: Timing out of range
- **WHEN** `raw` contains `70000` or `0`
- **THEN** the message is rejected

#### Scenario: Frame too long in time
- **WHEN** `raw` contains 40 elements of 60000 µs (2.4 s total)
- **THEN** the message is rejected

#### Scenario: Frequency out of range
- **WHEN** `freq` is `100000`
- **THEN** the message is rejected; the default frequency is not substituted

#### Scenario: Malformed JSON
- **WHEN** the payload is `{"raw":[560,560` (truncated)
- **THEN** the message is rejected

#### Scenario: Oversized payload
- **WHEN** the payload is 5000 bytes long
- **THEN** the message is rejected without being parsed

### Requirement: Rejection is reported in the log
For every rejected message, the firmware SHALL log a single line with the reason (e.g. "raw too long", "freq out of range") and SHALL continue normal operation.

#### Scenario: Rejected message
- **WHEN** an invalid message arrives on `<topic_base>/send`
- **THEN** one log line with the rejection reason is written, nothing is transmitted, and later valid messages are processed normally

### Requirement: Canonical serialization
Messages published by the firmware SHALL be serialized in the canonical compact form `{"freq":<n>,"raw":[<t1>,<t2>,...]}`, with no whitespace and `freq` first. Parsing a serialized frame SHALL return the original frequency and timings unchanged.

#### Scenario: Round trip
- **WHEN** a frame with freq 38000 and timings 9000, 4500, 560 is serialized and the result is parsed
- **THEN** the serialized text is `{"freq":38000,"raw":[9000,4500,560]}` and parsing returns freq 38000 and timings 9000, 4500, 560

#### Scenario: Maximum frame fits
- **WHEN** a frame with 512 five-digit timings and freq 60000 is serialized
- **THEN** the result fits within 4096 bytes and is complete valid JSON

