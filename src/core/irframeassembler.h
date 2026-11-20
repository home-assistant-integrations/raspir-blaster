/*
 *  irframeassembler.h
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

#ifndef _raspir_irframeassembler_h
#define _raspir_irframeassembler_h

#include "irmessage.h"
#include <stdint.h>

#define IR_MIN_FRAME_TIMINGS	4			// shorter frames are noise
#define IR_CAPTURE_MAX_EDGES	(IR_MAX_TIMINGS + 1)
#define IR_CAPTURE_BUFFERS	2

enum TIRCaptureResult
{
	IRCaptureFrame,		// valid frame
	IRCaptureNoise,		// fewer than IR_MIN_FRAME_TIMINGS timings - discard silently
	IRCaptureOverflow	// more than IR_MAX_TIMINGS timings or longer than 2 s
};

/// Splits receiver edge timestamps into frames (see specs/ir-receive).
///
/// OnEdge() runs in interrupt context and stores raw timestamps. A frame ends
/// when no edge occurs for the gap time; this is detected by OnEdge() itself
/// (on the first edge after the gap) or by TakeCompleted() (when the line stays
/// idle). Completed captures wait in one of two buffers until the main loop
/// converts and releases them, so frames are not lost while the main loop is
/// busy publishing or transmitting. If both buffers are full, a new frame is
/// dropped completely (never partially) and counted as lost.
///
/// Locking: OnEdge(), TakeCompleted(), Release(), SetMuted() and SetGapUs()
/// share state; the firmware disables interrupts around the main-loop calls.
/// Convert() only reads a completed capture, which OnEdge() never touches.
class CIRFrameAssembler
{
public:
	CIRFrameAssembler (void);

	void SetGapUs (uint32_t nGapUs);

	/// Interrupt context: an edge was seen at nTimestampUs (1 MHz clock, may wrap).
	void OnEdge (uint32_t nTimestampUs);

	/// Closes the frame in progress if the line has been idle for the gap time.
	/// \return index of the oldest completed capture, or -1 if there is none
	int TakeCompleted (uint32_t nNowUs);

	/// Converts a completed capture into a frame (carrier is reported as 38 kHz).
	TIRCaptureResult Convert (int nIndex, TIRFrame *pFrame) const;

	/// Returns a completed capture buffer for reuse.
	void Release (int nIndex);

	/// While muted, edges are ignored; muting drops a frame in progress.
	void SetMuted (bool bMuted);

	/// \return number of frames dropped because both buffers were in use
	unsigned GetLostFrames (void) const	{ return m_nLostFrames; }

private:
	struct TCapture
	{
		bool     bReady;
		uint32_t nSequence;
		unsigned nEdges;
		bool     bOverflow;
		uint32_t Timestamps[IR_CAPTURE_MAX_EDGES];
	};

	void Complete (int nIndex);
	int FindFree (void) const;
	void Reset (int nIndex);

private:
	TCapture m_Capture[IR_CAPTURE_BUFFERS];
	int      m_nFilling;		// capture receiving edges, -1 if dropping
	bool     m_bHaveLastEdge;
	uint32_t m_nLastEdgeUs;		// last edge seen on the line
	uint32_t m_nGapUs;
	uint32_t m_nNextSequence;
	bool     m_bMuted;
	unsigned m_nLostFrames;
};

#endif
