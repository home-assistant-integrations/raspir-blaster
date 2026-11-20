/*
 *  irmessage.h
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

#ifndef _raspir_irmessage_h
#define _raspir_irmessage_h

#include <stddef.h>
#include <stdint.h>

// Limits of the raw IR message format (see specs/ir-message-format).
#define IR_MAX_PAYLOAD		4096		// bytes
#define IR_MAX_TIMINGS		512
#define IR_MIN_TIMING_US	1
#define IR_MAX_TIMING_US	65535
#define IR_MAX_TOTAL_US		2000000		// 2 s
#define IR_DEFAULT_FREQ_HZ	38000
#define IR_MIN_FREQ_HZ		20000
#define IR_MAX_FREQ_HZ		60000

/// A raw IR frame: alternating mark/space durations in microseconds,
/// starting with a mark.
struct TIRFrame
{
	uint32_t nFreqHz;
	unsigned nCount;
	uint16_t Timings[IR_MAX_TIMINGS];
};

enum TIRParseResult
{
	IRParseOK,
	IRParsePayloadTooLarge,
	IRParseSyntaxError,
	IRParseDuplicateKey,
	IRParseUnsupportedValue,	// unknown key with object/array value
	IRParseRawMissing,
	IRParseRawNotArray,
	IRParseRawEmpty,
	IRParseRawTooLong,
	IRParseTimingInvalid,
	IRParseTotalTooLong,
	IRParseFreqInvalid
};

/// Parses and validates a message such as {"freq":38000,"raw":[9000,4500,560]}.
/// \param pPayload Message text (not NUL-terminated)
/// \param pFrame   Receives the frame; only valid if IRParseOK is returned
TIRParseResult IRMessageParse (const char *pPayload, size_t nLength, TIRFrame *pFrame);

/// \return a short human-readable reason for the result, e.g. "raw too long"
const char *IRParseResultString (TIRParseResult Result);

/// Writes the canonical compact form {"freq":<n>,"raw":[<t1>,...]}.
/// \return length of the message, or 0 if the buffer is too small
size_t IRMessageSerialize (const TIRFrame &Frame, char *pBuffer, size_t nSize);

/// \return sum of all timings in microseconds
uint32_t IRFrameDurationUs (const TIRFrame &Frame);

#endif
