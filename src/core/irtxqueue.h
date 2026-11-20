/*
 *  irtxqueue.h
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

#ifndef _raspir_irtxqueue_h
#define _raspir_irtxqueue_h

#include "irmessage.h"

#define IR_TX_QUEUE_DEPTH	4

/// FIFO of frames waiting for transmission (see specs/ir-transmit).
/// Not interrupt-safe: producer (MQTT task) and consumer (main task) run
/// under Circle's cooperative scheduler and never preempt each other.
class CIRTxQueue
{
public:
	CIRTxQueue (void);

	/// \return false if the queue is full (the frame is dropped)
	bool Push (const TIRFrame &Frame);

	/// \return false if the queue is empty
	bool Pop (TIRFrame *pFrame);

	unsigned GetCount (void) const		{ return m_nCount; }

private:
	TIRFrame m_Frames[IR_TX_QUEUE_DEPTH];
	unsigned m_nHead;
	unsigned m_nCount;
};

#endif
