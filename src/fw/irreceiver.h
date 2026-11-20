/*
 *  irreceiver.h
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

#ifndef _irreceiver_h
#define _irreceiver_h

#include "irframeassembler.h"
#include <circle/gpiomanager.h>
#include <circle/gpiopin.h>
#include <circle/types.h>

#define IR_RX_GPIO_PIN		23	// header pin 16, TSOP38238 output

/// IR receiver on GPIO23: timestamps every edge in an interrupt handler and
/// hands completed frames to the main loop (see specs/ir-receive).
class CIRReceiver
{
public:
	CIRReceiver (CGPIOManager *pGPIOManager);
	~CIRReceiver (void);

	boolean Initialize (unsigned nFrameGapMs);

	/// Main loop: fetches the oldest completed capture, if any.
	/// \return TRUE if *pResult (and for IRCaptureFrame *pFrame) is valid
	boolean Poll (TIRFrame *pFrame, TIRCaptureResult *pResult);

	/// Ignores the receiver (during own transmissions); drops a partial frame.
	void Mute (void);
	void Unmute (void);

	unsigned GetLostFrames (void);

private:
	static void InterruptHandler (void *pParam);

private:
	CGPIOPin          m_Pin;
	CIRFrameAssembler m_Assembler;
};

#endif
