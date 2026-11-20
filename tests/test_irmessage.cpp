/*
 *  test_irmessage.cpp
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
#include "irmessage.h"
#include <memory>
#include <string>

namespace
{
	std::unique_ptr<TIRFrame> NewFrame (void)
	{
		return std::unique_ptr<TIRFrame> (new TIRFrame ());
	}

	TIRParseResult Parse (const std::string &Text, TIRFrame *pFrame)
	{
		return IRMessageParse (Text.data (), Text.size (), pFrame);
	}

	TIRParseResult Parse (const std::string &Text)
	{
		auto Frame = NewFrame ();
		return Parse (Text, Frame.get ());
	}

	std::string RawArray (unsigned nCount, unsigned nValue)
	{
		std::string Text = "[";
		for (unsigned i = 0; i < nCount; i++)
		{
			Text += (i > 0 ? "," : "") + std::to_string (nValue);
		}
		return Text + "]";
	}
}

// --- 6.2 schema ---------------------------------------------------------------

TEST (MessageMinimalUsesDefaultFrequency)
{
	auto Frame = NewFrame ();
	CHECK_EQ (Parse ("{\"raw\":[9000,4500,560]}", Frame.get ()), IRParseOK);
	CHECK_EQ (Frame->nFreqHz, 38000u);
	CHECK_EQ (Frame->nCount, 3u);
	CHECK_EQ (Frame->Timings[0], 9000);
	CHECK_EQ (Frame->Timings[1], 4500);
	CHECK_EQ (Frame->Timings[2], 560);
}

TEST (MessageWithFrequency)
{
	auto Frame = NewFrame ();
	CHECK_EQ (Parse ("{\"freq\":36000,\"raw\":[889,889,1778]}", Frame.get ()), IRParseOK);
	CHECK_EQ (Frame->nFreqHz, 36000u);
	CHECK_EQ (Frame->nCount, 3u);
}

TEST (MessageReorderedKeysAndWhitespace)
{
	auto Frame = NewFrame ();
	CHECK_EQ (Parse ("{ \"raw\" : [ 560 ,\n 560 ] , \"freq\" : 40000 }", Frame.get ()), IRParseOK);
	CHECK_EQ (Frame->nFreqHz, 40000u);
	CHECK_EQ (Frame->nCount, 2u);
}

TEST (MessageIgnoresUnknownScalarKeys)
{
	CHECK_EQ (Parse ("{\"name\":\"tv_power\",\"raw\":[560],\"n\":1.5,\"b\":true,\"z\":null}"),
		  IRParseOK);
}

TEST (MessageRejectsUnknownKeyWithCompoundValue)
{
	CHECK_EQ (Parse ("{\"raw\":[560],\"meta\":{\"a\":1}}"), IRParseUnsupportedValue);
	CHECK_EQ (Parse ("{\"raw\":[560],\"list\":[1,2]}"), IRParseUnsupportedValue);
}

// --- 6.3 rejection rules --------------------------------------------------------

TEST (MessageRejectsOversizedPayloadBeforeParsing)
{
	std::string Text (5000, ' ');
	CHECK_EQ (Parse (Text), IRParsePayloadTooLarge);
}

TEST (MessageAcceptsPayloadOfExactly4096Bytes)
{
	std::string Text = "{\"raw\":[560]}";
	Text += std::string (4096 - Text.size (), ' ');
	CHECK_EQ (Text.size (), 4096u);
	CHECK_EQ (Parse (Text), IRParseOK);
}

TEST (MessageRawMissingOrWrongType)
{
	CHECK_EQ (Parse ("{\"freq\":38000}"), IRParseRawMissing);
	CHECK_EQ (Parse ("{}"), IRParseRawMissing);
	CHECK_EQ (Parse ("{\"raw\":560}"), IRParseRawNotArray);
	CHECK_EQ (Parse ("{\"raw\":{\"a\":1}}"), IRParseRawNotArray);
	CHECK_EQ (Parse ("{\"raw\":[]}"), IRParseRawEmpty);
}

TEST (MessageRawLengthLimit)
{
	CHECK_EQ (Parse ("{\"raw\":" + RawArray (512, 100) + "}"), IRParseOK);
	CHECK_EQ (Parse ("{\"raw\":" + RawArray (513, 100) + "}"), IRParseRawTooLong);
}

TEST (MessageTimingRange)
{
	CHECK_EQ (Parse ("{\"raw\":[1,65535]}"), IRParseOK);
	CHECK_EQ (Parse ("{\"raw\":[560,70000]}"), IRParseTimingInvalid);
	CHECK_EQ (Parse ("{\"raw\":[0]}"), IRParseTimingInvalid);
	CHECK_EQ (Parse ("{\"raw\":[-560]}"), IRParseTimingInvalid);
	CHECK_EQ (Parse ("{\"raw\":[560.5]}"), IRParseTimingInvalid);
	CHECK_EQ (Parse ("{\"raw\":[\"560\"]}"), IRParseTimingInvalid);
}

TEST (MessageTotalDurationLimit)
{
	// 40 x 60000 us = 2.4 s
	CHECK_EQ (Parse ("{\"raw\":" + RawArray (40, 60000) + "}"), IRParseTotalTooLong);
	// 40 x 50000 us = 2.0 s exactly
	CHECK_EQ (Parse ("{\"raw\":" + RawArray (40, 50000) + "}"), IRParseOK);
}

TEST (MessageFrequencyRange)
{
	CHECK_EQ (Parse ("{\"freq\":20000,\"raw\":[560]}"), IRParseOK);
	CHECK_EQ (Parse ("{\"freq\":60000,\"raw\":[560]}"), IRParseOK);
	CHECK_EQ (Parse ("{\"freq\":100000,\"raw\":[560]}"), IRParseFreqInvalid);
	CHECK_EQ (Parse ("{\"freq\":19999,\"raw\":[560]}"), IRParseFreqInvalid);
	CHECK_EQ (Parse ("{\"freq\":38000.5,\"raw\":[560]}"), IRParseFreqInvalid);
	CHECK_EQ (Parse ("{\"freq\":\"38000\",\"raw\":[560]}"), IRParseFreqInvalid);
	CHECK_EQ (Parse ("{\"freq\":[38000],\"raw\":[560]}"), IRParseFreqInvalid);
}

TEST (MessageMalformedJSON)
{
	CHECK_EQ (Parse ("{\"raw\":[560,560"), IRParseSyntaxError);
	CHECK_EQ (Parse ("{\"raw\":[560,560]"), IRParseSyntaxError);
	CHECK_EQ (Parse ("{\"raw\":[560,]}"), IRParseSyntaxError);
	CHECK_EQ (Parse ("{\"raw\" [560]}"), IRParseSyntaxError);
	CHECK_EQ (Parse ("{raw:[560]}"), IRParseSyntaxError);
	CHECK_EQ (Parse ("[560]"), IRParseSyntaxError);
	CHECK_EQ (Parse ("{\"raw\":[560]} x"), IRParseSyntaxError);
	CHECK_EQ (Parse ("{\"raw\":[560]}{}"), IRParseSyntaxError);
	CHECK_EQ (Parse (""), IRParseSyntaxError);
}

TEST (MessageSyntaxErrorBeatsSemanticError)
{
	// The out-of-range timing comes first, but the message is not even JSON.
	CHECK_EQ (Parse ("{\"raw\":[70000,560"), IRParseSyntaxError);
	CHECK_EQ (Parse ("{\"freq\":1,\"raw\":[560]"), IRParseSyntaxError);
}

TEST (MessageDuplicateKeys)
{
	CHECK_EQ (Parse ("{\"raw\":[560],\"raw\":[560]}"), IRParseDuplicateKey);
	CHECK_EQ (Parse ("{\"freq\":38000,\"freq\":36000,\"raw\":[560]}"), IRParseDuplicateKey);
}

TEST (MessageReasonsAreDistinct)
{
	const TIRParseResult Results[] = {
		IRParseOK, IRParsePayloadTooLarge, IRParseSyntaxError, IRParseDuplicateKey,
		IRParseUnsupportedValue, IRParseRawMissing, IRParseRawNotArray, IRParseRawEmpty,
		IRParseRawTooLong, IRParseTimingInvalid, IRParseTotalTooLong, IRParseFreqInvalid };
	const unsigned nResults = sizeof Results / sizeof Results[0];

	for (unsigned i = 0; i < nResults; i++)
	{
		for (unsigned j = i + 1; j < nResults; j++)
		{
			CHECK (std::string (IRParseResultString (Results[i]))
			       != IRParseResultString (Results[j]));
		}
	}

	CHECK_STR (IRParseResultString (IRParseRawTooLong), "raw too long");
	CHECK_STR (IRParseResultString (IRParseFreqInvalid), "freq out of range");
}

// --- 6.4 serialization ----------------------------------------------------------

TEST (MessageSerializeCanonicalForm)
{
	auto Frame = NewFrame ();
	Frame->nFreqHz = 38000;
	Frame->nCount = 3;
	Frame->Timings[0] = 9000;
	Frame->Timings[1] = 4500;
	Frame->Timings[2] = 560;

	char Buffer[64];
	size_t nLength = IRMessageSerialize (*Frame, Buffer, sizeof Buffer);

	CHECK_STR (Buffer, "{\"freq\":38000,\"raw\":[9000,4500,560]}");
	CHECK_EQ (nLength, std::strlen (Buffer));
}

TEST (MessageRoundTrip)
{
	auto Original = NewFrame ();
	Original->nFreqHz = 36000;
	Original->nCount = 5;
	const uint16_t Timings[] = { 889, 889, 1778, 1, 65535 };
	for (unsigned i = 0; i < 5; i++)
	{
		Original->Timings[i] = Timings[i];
	}

	char Buffer[IR_MAX_PAYLOAD];
	size_t nLength = IRMessageSerialize (*Original, Buffer, sizeof Buffer);

	auto Parsed = NewFrame ();
	CHECK_EQ (IRMessageParse (Buffer, nLength, Parsed.get ()), IRParseOK);
	CHECK_EQ (Parsed->nFreqHz, 36000u);
	CHECK_EQ (Parsed->nCount, 5u);
	for (unsigned i = 0; i < 5; i++)
	{
		CHECK_EQ (Parsed->Timings[i], Timings[i]);
	}
}

TEST (MessageWorstCaseFitsInPayloadLimit)
{
	auto Frame = NewFrame ();
	Frame->nFreqHz = 60000;
	Frame->nCount = IR_MAX_TIMINGS;
	for (unsigned i = 0; i < IR_MAX_TIMINGS; i++)
	{
		Frame->Timings[i] = 65535;
	}

	char Buffer[IR_MAX_PAYLOAD];
	size_t nLength = IRMessageSerialize (*Frame, Buffer, sizeof Buffer);

	CHECK (nLength > 0);
	CHECK (nLength < IR_MAX_PAYLOAD);
	// Complete JSON: the tokenizer-level syntax is valid (only the 2 s limit fails).
	auto Parsed = NewFrame ();
	CHECK_EQ (IRMessageParse (Buffer, nLength, Parsed.get ()), IRParseTotalTooLong);
	CHECK_EQ (Parsed->nCount, (unsigned) IR_MAX_TIMINGS);
}

TEST (MessageSerializeReportsOverflow)
{
	auto Frame = NewFrame ();
	Frame->nFreqHz = 38000;
	Frame->nCount = 3;
	Frame->Timings[0] = Frame->Timings[1] = Frame->Timings[2] = 560;

	char Buffer[16];
	CHECK_EQ (IRMessageSerialize (*Frame, Buffer, sizeof Buffer), 0u);
	CHECK_STR (Buffer, "");
}
