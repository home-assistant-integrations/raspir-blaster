/*
 *  test_logring.cpp
 *  raspir-blaster tests
 *
 *  Created by Thomas Bonk on 2026-11-21.
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

// Tests for the USB log ring buffer (connectivity spec, "Diagnostic log on
// serial": replay on terminal open, CR LF, lost bytes).

#include "test.h"
#include "logring.h"
#include <string>

static void AppendString (CLogRing *pRing, const std::string &Text)
{
	pRing->Append (Text.data (), Text.size ());
}

// Reads everything the reader can get, in chunks of at most nChunk bytes
// (like partial USB writes).
static std::string Drain (CLogRing *pRing, size_t nChunk = 100000)
{
	std::string Out;
	const char *pData;
	size_t nLength;
	while ((nLength = pRing->Peek (&pData)) > 0)
	{
		if (nLength > nChunk)
		{
			nLength = nChunk;
		}
		Out.append (pData, nLength);
		pRing->Consume (nLength);
	}
	return Out;
}

static CLogRing Ring;	// 16 KB, so not on the stack of every test

static void Reset (void)
{
	Ring = CLogRing ();
}

TEST (LogRingNotReadingPeeksNothing)
{
	Reset ();
	AppendString (&Ring, "hello\n");
	const char *pData;
	CHECK_EQ (Ring.Peek (&pData), 0u);
	CHECK (!Ring.IsReading ());
}

TEST (LogRingReplaysKeptTextAsCRLF)
{
	Reset ();
	AppendString (&Ring, "boot\nline 2\n");
	Ring.StartReader ();
	CHECK_STR (Drain (&Ring).c_str (), "boot\r\nline 2\r\n");
	CHECK_EQ (Ring.TakeLost (), 0u);
}

TEST (LogRingKeepsExistingCRLF)
{
	Reset ();
	AppendString (&Ring, "a\r\nb\n");
	Ring.StartReader ();
	CHECK_STR (Drain (&Ring).c_str (), "a\r\nb\r\n");
}

TEST (LogRingCRLFAcrossAppendBoundary)
{
	Reset ();
	AppendString (&Ring, "x\r");
	AppendString (&Ring, "\ny\n");	// CR of the previous append, no extra CR
	Ring.StartReader ();
	CHECK_STR (Drain (&Ring).c_str (), "x\r\ny\r\n");
}

TEST (LogRingFollowsLiveOutputAfterReplay)
{
	Reset ();
	AppendString (&Ring, "old\n");
	Ring.StartReader ();
	CHECK_STR (Drain (&Ring).c_str (), "old\r\n");
	AppendString (&Ring, "new\n");
	CHECK_STR (Drain (&Ring, 2).c_str (), "new\r\n");	// partial writes
}

TEST (LogRingWrapAroundKeepsNewestCapacityBytes)
{
	Reset ();
	std::string Text;
	for (unsigned i = 0; Text.size () < LOG_RING_SIZE + 5000; i++)
	{
		Text += "line " + std::to_string (i) + "\r\n";
	}
	AppendString (&Ring, Text);

	Ring.StartReader ();
	std::string Out = Drain (&Ring, 777);
	CHECK_EQ (Out.size (), (size_t) LOG_RING_SIZE);
	CHECK (Out == Text.substr (Text.size () - LOG_RING_SIZE));
	CHECK_EQ (Ring.TakeLost (), 0u);	// replay start is not a loss
}

TEST (LogRingOverrunWhileReadingCountsLostBytes)
{
	Reset ();
	Ring.StartReader ();
	AppendString (&Ring, std::string (LOG_RING_SIZE + 300, 'a'));

	CHECK_EQ (Ring.TakeLost (), 300u);
	CHECK_EQ (Ring.TakeLost (), 0u);	// taken once
	CHECK_EQ (Drain (&Ring).size (), (size_t) LOG_RING_SIZE);
}

TEST (LogRingRestartReplaysAgain)
{
	Reset ();
	AppendString (&Ring, "boot\n");
	Ring.StartReader ();
	Drain (&Ring);
	Ring.StopReader ();

	AppendString (&Ring, "later\n");	// no reader: nothing lost
	Ring.StartReader ();
	CHECK_EQ (Ring.TakeLost (), 0u);
	CHECK_STR (Drain (&Ring).c_str (), "boot\r\nlater\r\n");
}

TEST (LogRingConsumeNeverPassesWriter)
{
	Reset ();
	AppendString (&Ring, "ab");
	Ring.StartReader ();
	Ring.Consume (10);
	const char *pData;
	CHECK_EQ (Ring.Peek (&pData), 0u);
	AppendString (&Ring, "c");
	CHECK_STR (Drain (&Ring).c_str (), "c");
}
