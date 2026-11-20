/*
 *  irreceiver.cpp
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

#include "irreceiver.h"
#include <circle/synchronize.h>
#include <circle/timer.h>

CIRReceiver::CIRReceiver (CGPIOManager *pGPIOManager)
:	m_Pin (IR_RX_GPIO_PIN, GPIOModeInputPullUp, pGPIOManager)
{
}

CIRReceiver::~CIRReceiver (void)
{
	m_Pin.DisableInterrupt2 ();
	m_Pin.DisableInterrupt ();
	m_Pin.DisconnectInterrupt ();
}

boolean CIRReceiver::Initialize (unsigned nFrameGapMs)
{
	m_Assembler.SetGapUs (nFrameGapMs * 1000);

	// Circle has no "both edges" mode: use both interrupt sources of the pin.
	m_Pin.ConnectInterrupt (InterruptHandler, this);
	m_Pin.EnableInterrupt (GPIOInterruptOnRisingEdge);
	m_Pin.EnableInterrupt2 (GPIOInterruptOnFallingEdge);

	return TRUE;
}

boolean CIRReceiver::Poll (TIRFrame *pFrame, TIRCaptureResult *pResult)
{
	EnterCritical (IRQ_LEVEL);
	int nIndex = m_Assembler.TakeCompleted (CTimer::GetClockTicks ());
	LeaveCritical ();

	if (nIndex < 0)
	{
		return FALSE;
	}

	// A completed capture is never touched by the interrupt handler.
	*pResult = m_Assembler.Convert (nIndex, pFrame);

	EnterCritical (IRQ_LEVEL);
	m_Assembler.Release (nIndex);
	LeaveCritical ();

	return TRUE;
}

void CIRReceiver::Mute (void)
{
	EnterCritical (IRQ_LEVEL);
	m_Assembler.SetMuted (true);
	LeaveCritical ();
}

void CIRReceiver::Unmute (void)
{
	EnterCritical (IRQ_LEVEL);
	m_Assembler.SetMuted (false);
	LeaveCritical ();
}

unsigned CIRReceiver::GetLostFrames (void)
{
	EnterCritical (IRQ_LEVEL);
	unsigned nLost = m_Assembler.GetLostFrames ();
	LeaveCritical ();

	return nLost;
}

void CIRReceiver::InterruptHandler (void *pParam)
{
	CIRReceiver *pThis = static_cast<CIRReceiver *> (pParam);

	pThis->m_Assembler.OnEdge (CTimer::GetClockTicks ());
}
