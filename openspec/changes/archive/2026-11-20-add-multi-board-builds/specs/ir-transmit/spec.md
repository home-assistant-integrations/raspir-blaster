## MODIFIED Requirements

### Requirement: Hardware PWM carrier
During marks, the firmware SHALL generate the carrier on GPIO18 with the SoC's hardware PWM (PWM0) at the message's `freq`, with a duty cycle of 33 % ± 3 %. The PWM clock SHALL be derived from the SoC oscillator, 19.2 MHz on BCM2835/BCM2837 and 54 MHz on BCM2711, and the PWM range SHALL be computed for that clock. The carrier frequency SHALL be within ±1 % of the requested value. During spaces and when idle, GPIO18 SHALL be held low (LED off).

#### Scenario: Carrier frequency
- **WHEN** a message with `freq` 36000, 38000 or 40000 is transmitted
- **THEN** the measured carrier frequency during marks is within ±1 % of the requested value

#### Scenario: Carrier frequency on a Pi 4
- **WHEN** a message with `freq` 38000 is transmitted on a Pi 4 or CM4
- **THEN** the PWM range is computed for the 54 MHz oscillator and the measured carrier frequency is within ±1 % of 38000 Hz

#### Scenario: Idle state
- **WHEN** no transmission is in progress
- **THEN** GPIO18 is low
