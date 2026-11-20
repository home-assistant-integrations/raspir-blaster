/*
 *  irmessage.cpp
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

#include "irmessage.h"
#include "jsontokenizer.h"
#include "textwriter.h"

namespace
{
	bool IsScalar (TJSONTokenType Type)
	{
		return    Type == JSONTokenString || Type == JSONTokenNumber
		       || Type == JSONTokenTrue || Type == JSONTokenFalse || Type == JSONTokenNull;
	}

	/// Parses the elements of the "raw" array; the '[' has been consumed.
	TIRParseResult ParseRawArray (CJSONTokenizer &Tokenizer, TIRFrame *pFrame)
	{
		TJSONToken Token;
		bool bTimingInvalid = false;
		unsigned nCount = 0;

		if (Tokenizer.Next (&Token) == JSONTokenArrayEnd)
		{
			return IRParseRawEmpty;
		}

		for (;;)
		{
			if (!IsScalar (Token.Type))
			{
				return IRParseSyntaxError;
			}

			if (nCount >= IR_MAX_TIMINGS)
			{
				return IRParseRawTooLong;
			}

			if (   Token.Type != JSONTokenNumber || !Token.bIsUnsigned
			    || Token.nValue < IR_MIN_TIMING_US || Token.nValue > IR_MAX_TIMING_US)
			{
				// Keep scanning: a syntax error later in the message wins.
				bTimingInvalid = true;
			}
			else
			{
				pFrame->Timings[nCount] = (uint16_t) Token.nValue;
			}
			nCount++;

			Tokenizer.Next (&Token);
			if (Token.Type == JSONTokenArrayEnd)
			{
				break;
			}
			if (Token.Type != JSONTokenComma)
			{
				return IRParseSyntaxError;
			}
			Tokenizer.Next (&Token);
		}

		pFrame->nCount = nCount;
		return bTimingInvalid ? IRParseTimingInvalid : IRParseOK;
	}
}

TIRParseResult IRMessageParse (const char *pPayload, size_t nLength, TIRFrame *pFrame)
{
	if (nLength > IR_MAX_PAYLOAD)
	{
		return IRParsePayloadTooLarge;
	}

	CJSONTokenizer Tokenizer (pPayload, nLength);
	TJSONToken Token;

	bool bHaveRaw = false;
	bool bHaveFreq = false;
	// A semantic error is remembered, but a later syntax error takes precedence.
	TIRParseResult SemanticError = IRParseOK;

	pFrame->nFreqHz = IR_DEFAULT_FREQ_HZ;
	pFrame->nCount = 0;

	if (Tokenizer.Next (&Token) != JSONTokenObjectBegin)
	{
		return IRParseSyntaxError;
	}

	Tokenizer.Next (&Token);
	if (Token.Type != JSONTokenObjectEnd)
	{
		for (;;)
		{
			if (Token.Type != JSONTokenString)
			{
				return IRParseSyntaxError;
			}
			TJSONToken Key = Token;

			if (Tokenizer.Next (&Token) != JSONTokenColon)
			{
				return IRParseSyntaxError;
			}

			Tokenizer.Next (&Token);

			if (CoreStrEqualN (Key.pText, Key.nLength, "raw"))
			{
				if (bHaveRaw)
				{
					return IRParseDuplicateKey;
				}
				bHaveRaw = true;

				if (Token.Type == JSONTokenArrayBegin)
				{
					TIRParseResult Result = ParseRawArray (Tokenizer, pFrame);
					if (Result == IRParseSyntaxError || Result == IRParseRawTooLong)
					{
						return Result;
					}
					if (Result != IRParseOK && SemanticError == IRParseOK)
					{
						SemanticError = Result;
					}
				}
				else if (IsScalar (Token.Type))
				{
					if (SemanticError == IRParseOK)
					{
						SemanticError = IRParseRawNotArray;
					}
				}
				else
				{
					return Token.Type == JSONTokenObjectBegin ? IRParseRawNotArray
										  : IRParseSyntaxError;
				}
			}
			else if (CoreStrEqualN (Key.pText, Key.nLength, "freq"))
			{
				if (bHaveFreq)
				{
					return IRParseDuplicateKey;
				}
				bHaveFreq = true;

				if (!IsScalar (Token.Type))
				{
					return Token.Type == JSONTokenError ? IRParseSyntaxError
									    : IRParseFreqInvalid;
				}

				if (   Token.Type != JSONTokenNumber || !Token.bIsUnsigned
				    || Token.nValue < IR_MIN_FREQ_HZ || Token.nValue > IR_MAX_FREQ_HZ)
				{
					if (SemanticError == IRParseOK)
					{
						SemanticError = IRParseFreqInvalid;
					}
				}
				else
				{
					pFrame->nFreqHz = Token.nValue;
				}
			}
			else
			{
				// Unknown keys are ignored, but only scalar values are supported.
				if (!IsScalar (Token.Type))
				{
					return    Token.Type == JSONTokenObjectBegin
					       || Token.Type == JSONTokenArrayBegin
						? IRParseUnsupportedValue : IRParseSyntaxError;
				}
			}

			Tokenizer.Next (&Token);
			if (Token.Type == JSONTokenObjectEnd)
			{
				break;
			}
			if (Token.Type != JSONTokenComma)
			{
				return IRParseSyntaxError;
			}
			Tokenizer.Next (&Token);
		}
	}

	if (Tokenizer.Next (&Token) != JSONTokenEnd)
	{
		return IRParseSyntaxError;		// trailing garbage
	}

	if (SemanticError != IRParseOK)
	{
		return SemanticError;
	}

	if (!bHaveRaw)
	{
		return IRParseRawMissing;
	}

	if (IRFrameDurationUs (*pFrame) > IR_MAX_TOTAL_US)
	{
		return IRParseTotalTooLong;
	}

	return IRParseOK;
}

const char *IRParseResultString (TIRParseResult Result)
{
	switch (Result)
	{
	case IRParseOK:			return "ok";
	case IRParsePayloadTooLarge:	return "payload too large";
	case IRParseSyntaxError:	return "malformed JSON";
	case IRParseDuplicateKey:	return "duplicate key";
	case IRParseUnsupportedValue:	return "unsupported value for unknown key";
	case IRParseRawMissing:		return "raw missing";
	case IRParseRawNotArray:	return "raw is not an array";
	case IRParseRawEmpty:		return "raw is empty";
	case IRParseRawTooLong:		return "raw too long";
	case IRParseTimingInvalid:	return "timing out of range";
	case IRParseTotalTooLong:	return "frame longer than 2 s";
	case IRParseFreqInvalid:	return "freq out of range";
	}

	return "unknown error";
}

size_t IRMessageSerialize (const TIRFrame &Frame, char *pBuffer, size_t nSize)
{
	CTextWriter Writer (pBuffer, nSize);

	Writer.Append ("{\"freq\":");
	Writer.AppendUnsigned (Frame.nFreqHz);
	Writer.Append (",\"raw\":[");

	for (unsigned i = 0; i < Frame.nCount; i++)
	{
		if (i > 0)
		{
			Writer.AppendChar (',');
		}
		Writer.AppendUnsigned (Frame.Timings[i]);
	}

	Writer.Append ("]}");

	if (Writer.Overflow ())
	{
		if (nSize > 0)
		{
			pBuffer[0] = '\0';
		}
		return 0;
	}

	return Writer.GetLength ();
}

uint32_t IRFrameDurationUs (const TIRFrame &Frame)
{
	uint32_t nTotal = 0;
	for (unsigned i = 0; i < Frame.nCount; i++)
	{
		nTotal += Frame.Timings[i];
	}

	return nTotal;
}
