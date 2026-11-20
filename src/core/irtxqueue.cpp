/*
 *  irtxqueue.cpp
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

#include "irtxqueue.h"

namespace
{
	void CopyFrame (const TIRFrame &Source, TIRFrame *pDest)
	{
		pDest->nFreqHz = Source.nFreqHz;
		pDest->nCount = Source.nCount;
		for (unsigned i = 0; i < Source.nCount; i++)
		{
			pDest->Timings[i] = Source.Timings[i];
		}
	}
}

CIRTxQueue::CIRTxQueue (void)
:	m_nHead (0),
	m_nCount (0)
{
}

bool CIRTxQueue::Push (const TIRFrame &Frame)
{
	if (m_nCount >= IR_TX_QUEUE_DEPTH)
	{
		return false;
	}

	CopyFrame (Frame, &m_Frames[(m_nHead + m_nCount) % IR_TX_QUEUE_DEPTH]);
	m_nCount++;

	return true;
}

bool CIRTxQueue::Pop (TIRFrame *pFrame)
{
	if (m_nCount == 0)
	{
		return false;
	}

	CopyFrame (m_Frames[m_nHead], pFrame);
	m_nHead = (m_nHead + 1) % IR_TX_QUEUE_DEPTH;
	m_nCount--;

	return true;
}
