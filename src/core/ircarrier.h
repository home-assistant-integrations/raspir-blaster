/*
 *  ircarrier.h
 *  raspir-blaster core
 *
 *  Created by Thomas Bonk on 2026-11-20.
 *  Copyright 2026 Thomas Bonk <thomas@meandmymac.de>
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 */

#ifndef _raspir_ircarrier_h
#define _raspir_ircarrier_h

#include <stdint.h>

// The PWM clock is the SoC oscillator divided by IR_PWM_CLOCK_DIVIDER.
#define IR_OSCILLATOR_BCM283X_HZ	19200000	// BCM2835, BCM2836, BCM2837
#define IR_OSCILLATOR_BCM2711_HZ	54000000	// Pi 4, CM4
#define IR_PWM_CLOCK_DIVIDER		2

/// PWM settings that produce a carrier with ~33 % duty cycle.
struct TIRCarrier
{
	unsigned nRange;		// PWM range register (period in PWM clock ticks)
	unsigned nMarkValue;		// PWM data register value during a mark
	uint32_t nActualFreqHz;		// resulting carrier frequency
};

/// Computes the PWM settings for a carrier frequency.
/// \param nOscillatorHz SoC oscillator feeding the PWM clock (before the divider)
/// \return false if nFreqHz is outside IR_MIN_FREQ_HZ..IR_MAX_FREQ_HZ
bool IRCarrierCompute (uint32_t nFreqHz, uint32_t nOscillatorHz, TIRCarrier *pCarrier);

#endif
