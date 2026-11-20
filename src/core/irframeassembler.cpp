/*
 *  irframeassembler.cpp
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

#include "irframeassembler.h"

CIRFrameAssembler::CIRFrameAssembler (void)
:	m_nFilling (0),
	m_bHaveLastEdge (false),
	m_nLastEdgeUs (0),
	m_nGapUs (15000),
	m_nNextSequence (0),
	m_bMuted (false),
	m_nLostFrames (0)
{
	for (int i = 0; i < IR_CAPTURE_BUFFERS; i++)
	{
		m_Capture[i].bReady = false;
		m_Capture[i].nSequence = 0;
		Reset (i);
	}
}

void CIRFrameAssembler::SetGapUs (uint32_t nGapUs)
{
	m_nGapUs = nGapUs;
}

void CIRFrameAssembler::OnEdge (uint32_t nTimestampUs)
{
	if (m_bMuted)
	{
		return;
	}

	bool bGap = !m_bHaveLastEdge || nTimestampUs - m_nLastEdgeUs >= m_nGapUs;
	m_bHaveLastEdge = true;
	m_nLastEdgeUs = nTimestampUs;

	if (bGap)
	{
		// This edge starts a new frame.
		if (m_nFilling >= 0 && m_Capture[m_nFilling].nEdges > 0)
		{
			Complete (m_nFilling);
			m_nFilling = -1;
		}

		if (m_nFilling < 0)
		{
			m_nFilling = FindFree ();
			if (m_nFilling < 0)
			{
				m_nLostFrames++;	// drop the whole frame up to the next gap
				return;
			}
			Reset (m_nFilling);
		}
	}
	else if (m_nFilling < 0)
	{
		return;				// rest of a dropped frame
	}

	TCapture &Capture = m_Capture[m_nFilling];
	if (Capture.nEdges < IR_CAPTURE_MAX_EDGES)
	{
		Capture.Timestamps[Capture.nEdges++] = nTimestampUs;
	}
	else
	{
		Capture.bOverflow = true;
	}
}

int CIRFrameAssembler::TakeCompleted (uint32_t nNowUs)
{
	if (   m_nFilling >= 0
	    && m_Capture[m_nFilling].nEdges > 0
	    && nNowUs - m_nLastEdgeUs >= m_nGapUs)
	{
		Complete (m_nFilling);

		m_nFilling = FindFree ();
		if (m_nFilling >= 0)
		{
			Reset (m_nFilling);
		}
	}

	int nOldest = -1;
	for (int i = 0; i < IR_CAPTURE_BUFFERS; i++)
	{
		if (   m_Capture[i].bReady
		    && (   nOldest < 0
			|| (int32_t) (m_Capture[i].nSequence - m_Capture[nOldest].nSequence) < 0))
		{
			nOldest = i;
		}
	}

	return nOldest;
}

TIRCaptureResult CIRFrameAssembler::Convert (int nIndex, TIRFrame *pFrame) const
{
	const TCapture &Capture = m_Capture[nIndex];

	pFrame->nFreqHz = IR_DEFAULT_FREQ_HZ;
	pFrame->nCount = 0;

	if (Capture.bOverflow)
	{
		return IRCaptureOverflow;
	}

	if (Capture.nEdges < IR_MIN_FRAME_TIMINGS + 1)
	{
		return IRCaptureNoise;
	}

	if (Capture.Timestamps[Capture.nEdges - 1] - Capture.Timestamps[0] > IR_MAX_TOTAL_US)
	{
		return IRCaptureOverflow;
	}

	for (unsigned i = 1; i < Capture.nEdges; i++)
	{
		uint32_t nDelta = Capture.Timestamps[i] - Capture.Timestamps[i - 1];
		if (nDelta > IR_MAX_TIMING_US)
		{
			return IRCaptureOverflow;	// only possible with a gap > 65 ms
		}
		if (nDelta < IR_MIN_TIMING_US)
		{
			nDelta = IR_MIN_TIMING_US;	// two edges within the same microsecond
		}
		pFrame->Timings[i - 1] = (uint16_t) nDelta;
	}
	pFrame->nCount = Capture.nEdges - 1;

	return IRCaptureFrame;
}

void CIRFrameAssembler::Release (int nIndex)
{
	if (nIndex >= 0 && nIndex < IR_CAPTURE_BUFFERS)
	{
		m_Capture[nIndex].bReady = false;
		Reset (nIndex);
	}
}

void CIRFrameAssembler::SetMuted (bool bMuted)
{
	if (bMuted)
	{
		if (m_nFilling >= 0)
		{
			Reset (m_nFilling);		// drop the frame in progress
		}
	}
	else
	{
		// The next edge starts a new frame.
		m_bHaveLastEdge = false;
		if (m_nFilling < 0)
		{
			m_nFilling = FindFree ();
			if (m_nFilling >= 0)
			{
				Reset (m_nFilling);
			}
		}
	}

	m_bMuted = bMuted;
}

void CIRFrameAssembler::Complete (int nIndex)
{
	m_Capture[nIndex].bReady = true;
	m_Capture[nIndex].nSequence = m_nNextSequence++;
}

int CIRFrameAssembler::FindFree (void) const
{
	for (int i = 0; i < IR_CAPTURE_BUFFERS; i++)
	{
		if (!m_Capture[i].bReady)
		{
			return i;
		}
	}

	return -1;
}

void CIRFrameAssembler::Reset (int nIndex)
{
	m_Capture[nIndex].nEdges = 0;
	m_Capture[nIndex].bOverflow = false;
}
