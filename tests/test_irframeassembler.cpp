/*
 *  test_irframeassembler.cpp
 *  raspir-blaster tests
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

#include "test.h"
#include "irframeassembler.h"
#include <memory>
#include <vector>

namespace
{
	/// Feeds a signal into the assembler like the GPIO interrupt would.
	class CSignal
	{
	public:
		CSignal (CIRFrameAssembler *pAssembler, uint32_t nStartUs)
		:	m_pAssembler (pAssembler), m_nNowUs (nStartUs) {}

		/// Emits edges for the given mark/space durations, starting with an edge.
		void Send (const std::vector<uint32_t> &Timings)
		{
			m_pAssembler->OnEdge (m_nNowUs);
			for (uint32_t nTiming : Timings)
			{
				m_nNowUs += nTiming;
				m_pAssembler->OnEdge (m_nNowUs);
			}
		}

		void Wait (uint32_t nUs)		{ m_nNowUs += nUs; }
		uint32_t Now (void) const		{ return m_nNowUs; }

	private:
		CIRFrameAssembler *m_pAssembler;
		uint32_t           m_nNowUs;
	};

	struct TFixture
	{
		std::unique_ptr<CIRFrameAssembler> Assembler { new CIRFrameAssembler () };
		std::unique_ptr<TIRFrame>          Frame { new TIRFrame () };
	};

	const std::vector<uint32_t> NECStart = { 9000, 4500, 560, 560, 560, 1690, 560 };

	std::vector<uint32_t> FrameTimings (const TIRFrame &Frame)
	{
		return std::vector<uint32_t> (Frame.Timings, Frame.Timings + Frame.nCount);
	}
}

TEST (AssemblerSingleFrame)
{
	TFixture F;
	CSignal Signal (F.Assembler.get (), 1000);

	Signal.Send (NECStart);
	CHECK_EQ (F.Assembler->TakeCompleted (Signal.Now () + 14999), -1);	// gap not over yet

	Signal.Wait (100000);
	int nIndex = F.Assembler->TakeCompleted (Signal.Now ());
	CHECK (nIndex >= 0);
	CHECK_EQ (F.Assembler->Convert (nIndex, F.Frame.get ()), IRCaptureFrame);
	CHECK_EQ (F.Frame->nFreqHz, 38000u);
	CHECK (FrameTimings (*F.Frame) == NECStart);
	CHECK_EQ (F.Frame->nCount % 2, 1u);		// ends with a mark

	F.Assembler->Release (nIndex);
	CHECK_EQ (F.Assembler->TakeCompleted (Signal.Now () + 100000), -1);	// exactly one frame
}

TEST (AssemblerRepeatFramesWhileMainLoopBusy)
{
	TFixture F;
	CSignal Signal (F.Assembler.get (), 0);
	const std::vector<uint32_t> Repeat = { 9000, 2250, 560 , 560, 560 };

	// Both frames arrive before the main loop looks at the assembler.
	Signal.Send (NECStart);
	Signal.Wait (40000);
	Signal.Send (Repeat);
	Signal.Wait (100000);

	int nFirst = F.Assembler->TakeCompleted (Signal.Now ());
	CHECK (nFirst >= 0);
	CHECK_EQ (F.Assembler->Convert (nFirst, F.Frame.get ()), IRCaptureFrame);
	CHECK (FrameTimings (*F.Frame) == NECStart);
	F.Assembler->Release (nFirst);

	int nSecond = F.Assembler->TakeCompleted (Signal.Now ());
	CHECK (nSecond >= 0);
	CHECK_EQ (F.Assembler->Convert (nSecond, F.Frame.get ()), IRCaptureFrame);
	CHECK (FrameTimings (*F.Frame) == Repeat);
	F.Assembler->Release (nSecond);

	CHECK_EQ (F.Assembler->GetLostFrames (), 0u);
}

TEST (AssemblerDropsWholeFrameWhenBuffersFull)
{
	TFixture F;
	CSignal Signal (F.Assembler.get (), 0);
	const std::vector<uint32_t> Third = { 1000, 1000, 1000, 1000, 1000 };
	const std::vector<uint32_t> Fourth = { 2000, 2000, 2000, 2000, 2000 };

	Signal.Send (NECStart);
	Signal.Wait (30000);
	Signal.Send (NECStart);
	Signal.Wait (30000);
	Signal.Send (Third);			// both buffers occupied: lost
	CHECK_EQ (F.Assembler->GetLostFrames (), 1u);

	// The main loop frees a buffer while the third frame is still... over.
	Signal.Wait (30000);
	int nIndex = F.Assembler->TakeCompleted (Signal.Now ());
	F.Assembler->Release (nIndex);

	Signal.Send (Fourth);
	Signal.Wait (30000);

	nIndex = F.Assembler->TakeCompleted (Signal.Now ());	// second NEC frame
	F.Assembler->Release (nIndex);
	nIndex = F.Assembler->TakeCompleted (Signal.Now ());
	CHECK (nIndex >= 0);
	CHECK_EQ (F.Assembler->Convert (nIndex, F.Frame.get ()), IRCaptureFrame);
	CHECK (FrameTimings (*F.Frame) == Fourth);
}

TEST (AssemblerNeverDeliversTailOfDroppedFrame)
{
	TFixture F;
	CSignal Signal (F.Assembler.get (), 0);

	Signal.Send (NECStart);
	Signal.Wait (30000);
	Signal.Send (NECStart);
	Signal.Wait (30000);

	// Third frame starts while both buffers are occupied ...
	F.Assembler->OnEdge (Signal.Now ());
	Signal.Wait (9000);
	// ... a buffer becomes free in the middle of it ...
	F.Assembler->Release (F.Assembler->TakeCompleted (Signal.Now ()));
	// ... and the rest of the third frame must not become a frame.
	Signal.Send ({ 4500, 560, 560, 560, 560 });
	Signal.Wait (30000);

	int nIndex = F.Assembler->TakeCompleted (Signal.Now ());
	F.Assembler->Release (nIndex);		// second NEC frame
	CHECK_EQ (F.Assembler->TakeCompleted (Signal.Now ()), -1);
	CHECK_EQ (F.Assembler->GetLostFrames (), 1u);
}

TEST (AssemblerNoiseIsDiscarded)
{
	TFixture F;
	CSignal Signal (F.Assembler.get (), 0);

	Signal.Send ({ 300 });			// one spurious pulse: 1 timing
	Signal.Wait (50000);
	int nIndex = F.Assembler->TakeCompleted (Signal.Now ());
	CHECK (nIndex >= 0);
	CHECK_EQ (F.Assembler->Convert (nIndex, F.Frame.get ()), IRCaptureNoise);

	F.Assembler->Release (nIndex);
	Signal.Send ({ 560, 560, 560 });	// 3 timings: still noise
	Signal.Wait (50000);
	nIndex = F.Assembler->TakeCompleted (Signal.Now ());
	CHECK_EQ (F.Assembler->Convert (nIndex, F.Frame.get ()), IRCaptureNoise);

	F.Assembler->Release (nIndex);
	Signal.Send ({ 560, 560, 560, 560 });	// 4 timings: a frame
	Signal.Wait (50000);
	nIndex = F.Assembler->TakeCompleted (Signal.Now ());
	CHECK_EQ (F.Assembler->Convert (nIndex, F.Frame.get ()), IRCaptureFrame);
}

TEST (AssemblerTimingCountLimit)
{
	TFixture F;
	CSignal Signal (F.Assembler.get (), 0);

	Signal.Send (std::vector<uint32_t> (IR_MAX_TIMINGS, 500));
	Signal.Wait (50000);
	int nIndex = F.Assembler->TakeCompleted (Signal.Now ());
	CHECK_EQ (F.Assembler->Convert (nIndex, F.Frame.get ()), IRCaptureFrame);
	CHECK_EQ (F.Frame->nCount, (unsigned) IR_MAX_TIMINGS);
	F.Assembler->Release (nIndex);

	Signal.Send (std::vector<uint32_t> (IR_MAX_TIMINGS + 1, 500));
	Signal.Wait (50000);
	nIndex = F.Assembler->TakeCompleted (Signal.Now ());
	CHECK_EQ (F.Assembler->Convert (nIndex, F.Frame.get ()), IRCaptureOverflow);
}

TEST (AssemblerDurationLimit)
{
	TFixture F;
	F.Assembler->SetGapUs (60000);
	CSignal Signal (F.Assembler.get (), 0);

	// 50 timings of 45 ms = 2.25 s, no single pause reaches the 60 ms gap.
	Signal.Send (std::vector<uint32_t> (50, 45000));
	Signal.Wait (100000);
	int nIndex = F.Assembler->TakeCompleted (Signal.Now ());
	CHECK_EQ (F.Assembler->Convert (nIndex, F.Frame.get ()), IRCaptureOverflow);
}

TEST (AssemblerConfiguredGapKeepsInternalPause)
{
	TFixture F;
	F.Assembler->SetGapUs (30000);
	CSignal Signal (F.Assembler.get (), 0);
	const std::vector<uint32_t> AC = { 3500, 1750, 450, 1300, 450, 20000, 3500, 1750, 450 };

	Signal.Send (AC);
	Signal.Wait (100000);
	int nIndex = F.Assembler->TakeCompleted (Signal.Now ());
	CHECK_EQ (F.Assembler->Convert (nIndex, F.Frame.get ()), IRCaptureFrame);
	CHECK (FrameTimings (*F.Frame) == AC);
}

TEST (AssemblerDefaultGapSplitsAtPause)
{
	TFixture F;
	CSignal Signal (F.Assembler.get (), 0);

	Signal.Send ({ 3500, 1750, 450, 1300, 450, 20000, 3500, 1750, 450, 450, 450 });
	Signal.Wait (100000);

	int nFirst = F.Assembler->TakeCompleted (Signal.Now ());
	CHECK_EQ (F.Assembler->Convert (nFirst, F.Frame.get ()), IRCaptureFrame);
	CHECK_EQ (F.Frame->nCount, 5u);
	F.Assembler->Release (nFirst);

	int nSecond = F.Assembler->TakeCompleted (Signal.Now ());
	CHECK_EQ (F.Assembler->Convert (nSecond, F.Frame.get ()), IRCaptureFrame);
	CHECK_EQ (F.Frame->nCount, 5u);
}

TEST (AssemblerHandlesClockWrapAround)
{
	TFixture F;
	CSignal Signal (F.Assembler.get (), 0xFFFFFFFFu - 10000);

	Signal.Send (NECStart);			// crosses the 32-bit wrap
	Signal.Wait (50000);
	int nIndex = F.Assembler->TakeCompleted (Signal.Now ());
	CHECK (nIndex >= 0);
	CHECK_EQ (F.Assembler->Convert (nIndex, F.Frame.get ()), IRCaptureFrame);
	CHECK (FrameTimings (*F.Frame) == NECStart);
}

TEST (AssemblerMuteDropsPartialFrameAndIgnoresEdges)
{
	TFixture F;
	CSignal Signal (F.Assembler.get (), 0);

	Signal.Send ({ 9000, 4500 });		// partial frame
	F.Assembler->SetMuted (true);
	Signal.Send (NECStart);			// our own transmission
	Signal.Wait (5000);
	F.Assembler->SetMuted (false);
	CHECK_EQ (F.Assembler->TakeCompleted (Signal.Now () + 100000), -1);

	Signal.Wait (100000);
	Signal.Send (NECStart);
	Signal.Wait (100000);
	int nIndex = F.Assembler->TakeCompleted (Signal.Now ());
	CHECK_EQ (F.Assembler->Convert (nIndex, F.Frame.get ()), IRCaptureFrame);
	CHECK (FrameTimings (*F.Frame) == NECStart);
}

TEST (AssemblerUnmuteStartsNewFrameEvenWithoutGap)
{
	TFixture F;
	CSignal Signal (F.Assembler.get (), 0);

	F.Assembler->SetMuted (true);
	Signal.Send ({ 560, 560 });
	F.Assembler->SetMuted (false);
	Signal.Wait (1000);			// shorter than the gap
	Signal.Send (NECStart);
	Signal.Wait (100000);

	int nIndex = F.Assembler->TakeCompleted (Signal.Now ());
	CHECK_EQ (F.Assembler->Convert (nIndex, F.Frame.get ()), IRCaptureFrame);
	CHECK (FrameTimings (*F.Frame) == NECStart);
}
