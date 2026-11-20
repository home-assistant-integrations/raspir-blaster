/*
 *  irtransmitter.cpp
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

#include "irtransmitter.h"
#include <circle/gpioclock.h>
#include <circle/logger.h>
#include <circle/timer.h>

// The PWM clock source (oscillator) differs between the SoC generations.
#if RASPPI >= 4
	#define IR_OSCILLATOR_HZ	IR_OSCILLATOR_BCM2711_HZ
#else
	#define IR_OSCILLATOR_HZ	IR_OSCILLATOR_BCM283X_HZ
#endif

static const char FromTransmitter[] = "irtx";

CIRTransmitter::CIRTransmitter (void)
:	m_Pin (IR_TX_GPIO_PIN, GPIOModeOutput),
	m_pPWM (0),
	m_nFreqHz (0)
{
	m_Pin.Write (LOW);		// LED off until the PWM is configured
}

CIRTransmitter::~CIRTransmitter (void)
{
	m_Pin.SetMode (GPIOModeOutput);
	m_Pin.Write (LOW);

	delete m_pPWM;
	m_pPWM = 0;
}

boolean CIRTransmitter::Initialize (void)
{
	return SetFrequency (IR_DEFAULT_FREQ_HZ);
}

boolean CIRTransmitter::SetFrequency (uint32_t nFreqHz)
{
	if (m_pPWM != 0 && nFreqHz == m_nFreqHz)
	{
		return TRUE;
	}

	TIRCarrier Carrier;
	if (!IRCarrierCompute (nFreqHz, IR_OSCILLATOR_HZ, &Carrier))
	{
		return FALSE;
	}

	// Hold the pin low while the PWM is reconfigured.
	m_Pin.SetMode (GPIOModeOutput);
	m_Pin.Write (LOW);

	delete m_pPWM;
	m_pPWM = 0;
	m_nFreqHz = 0;

	// Mark/space mode: one high pulse of nMarkValue ticks per nRange ticks.
	m_pPWM = new CPWMOutput (GPIOClockSourceOscillator, IR_PWM_CLOCK_DIVIDER,
				 Carrier.nRange, TRUE);
	if (m_pPWM == 0 || !m_pPWM->Start ())
	{
		CLogger::Get ()->Write (FromTransmitter, LogError, "Cannot start PWM");
		delete m_pPWM;
		m_pPWM = 0;
		return FALSE;
	}

	m_pPWM->Write (IR_TX_PWM_CHANNEL, 0);
	m_Pin.SetMode (GPIOModeAlternateFunction5);	// GPIO18 = PWM0

	m_Carrier = Carrier;
	m_nFreqHz = nFreqHz;

	CLogger::Get ()->Write (FromTransmitter, LogDebug, "Carrier %u Hz (range %u, mark %u)",
				Carrier.nActualFreqHz, Carrier.nRange, Carrier.nMarkValue);

	return TRUE;
}

boolean CIRTransmitter::Send (const TIRFrame &Frame)
{
	if (!SetFrequency (Frame.nFreqHz))
	{
		return FALSE;
	}

	const unsigned nMarkValue = m_Carrier.nMarkValue;

	// Absolute deadlines: interrupt latency causes local jitter only, the
	// error does not accumulate over the frame.
	unsigned nDeadline = CTimer::GetClockTicks ();

	for (unsigned i = 0; i < Frame.nCount; i++)
	{
		m_pPWM->Write (IR_TX_PWM_CHANNEL, (i & 1) == 0 ? nMarkValue : 0);

		nDeadline += Frame.Timings[i];
		while ((int) (CTimer::GetClockTicks () - nDeadline) < 0)
		{
			// busy wait
		}
	}

	m_pPWM->Write (IR_TX_PWM_CHANNEL, 0);

	return TRUE;
}
