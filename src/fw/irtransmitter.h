/*
 *  irtransmitter.h
 *  raspir-blaster firmware
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

#ifndef _irtransmitter_h
#define _irtransmitter_h

#include "ircarrier.h"
#include "irmessage.h"
#include <circle/gpiopin.h>
#include <circle/pwmoutput.h>
#include <circle/types.h>

#define IR_TX_GPIO_PIN		18		// header pin 12, PWM0 (ALT5)
#define IR_TX_PWM_CHANNEL	PWM_CHANNEL1	// PWM0 drives GPIO18

/// IR transmitter on GPIO18: the hardware PWM generates the carrier, marks
/// and spaces are gated against absolute deadlines (see specs/ir-transmit).
class CIRTransmitter
{
public:
	CIRTransmitter (void);
	~CIRTransmitter (void);

	boolean Initialize (void);

	/// Transmits one frame. Busy-waits for the duration of the frame (<= 2 s);
	/// interrupts stay enabled.
	boolean Send (const TIRFrame &Frame);

private:
	/// (Re)creates the PWM output for a carrier frequency (cached).
	boolean SetFrequency (uint32_t nFreqHz);

private:
	CGPIOPin    m_Pin;
	CPWMOutput *m_pPWM;
	uint32_t    m_nFreqHz;
	TIRCarrier  m_Carrier;
};

#endif
