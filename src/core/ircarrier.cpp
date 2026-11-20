/*
 *  ircarrier.cpp
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

#include "ircarrier.h"
#include "irmessage.h"

bool IRCarrierCompute (uint32_t nFreqHz, uint32_t nOscillatorHz, TIRCarrier *pCarrier)
{
	if (nFreqHz < IR_MIN_FREQ_HZ || nFreqHz > IR_MAX_FREQ_HZ)
	{
		return false;
	}

	const uint32_t nClockHz = nOscillatorHz / IR_PWM_CLOCK_DIVIDER;

	// Round to the nearest range, then to the nearest third of it.
	unsigned nRange = (nClockHz + nFreqHz / 2) / nFreqHz;

	pCarrier->nRange = nRange;
	pCarrier->nMarkValue = (nRange + 1) / 3;
	pCarrier->nActualFreqHz = (nClockHz + nRange / 2) / nRange;

	return true;
}
