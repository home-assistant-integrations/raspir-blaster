/*
 *  test_jsontokenizer.cpp
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
#include "jsontokenizer.h"
#include <string>
#include <vector>

namespace
{
	std::vector<TJSONTokenType> Types (const char *pText)
	{
		CJSONTokenizer Tokenizer (pText, std::strlen (pText));
		std::vector<TJSONTokenType> Result;
		TJSONToken Token;

		for (;;)
		{
			TJSONTokenType Type = Tokenizer.Next (&Token);
			Result.push_back (Type);
			if (Type == JSONTokenEnd || Type == JSONTokenError || Result.size () > 64)
			{
				return Result;
			}
		}
	}

	TJSONToken Single (const char *pText)
	{
		CJSONTokenizer Tokenizer (pText, std::strlen (pText));
		TJSONToken Token;
		Tokenizer.Next (&Token);
		return Token;
	}
}

TEST (TokenizerStructuralTokensAndWhitespace)
{
	std::vector<TJSONTokenType> Expected = {
		JSONTokenObjectBegin, JSONTokenString, JSONTokenColon, JSONTokenArrayBegin,
		JSONTokenNumber, JSONTokenComma, JSONTokenNumber, JSONTokenArrayEnd,
		JSONTokenObjectEnd, JSONTokenEnd };

	CHECK (Types (" {\t\"raw\"\r\n:[ 1 ,\n2 ] } ") == Expected);
}

TEST (TokenizerLiterals)
{
	std::vector<TJSONTokenType> Expected = {
		JSONTokenTrue, JSONTokenFalse, JSONTokenNull, JSONTokenEnd };

	CHECK (Types ("true false null") == Expected);
	CHECK (Types ("tru").back () == JSONTokenError);
	CHECK (Types ("nul").back () == JSONTokenError);
}

TEST (TokenizerStringContents)
{
	TJSONToken Token = Single ("\"tv_power\"");

	CHECK_EQ (Token.Type, JSONTokenString);
	CHECK_STR (std::string (Token.pText, Token.nLength), "tv_power");
}

TEST (TokenizerStringEscapes)
{
	CHECK_EQ (Single ("\"a\\\"b\\\\c\\/d\\n\\u00e9\"").Type, JSONTokenString);
	CHECK_EQ (Single ("\"bad \\x escape\"").Type, JSONTokenError);
	CHECK_EQ (Single ("\"short \\u12\"").Type, JSONTokenError);
	CHECK_EQ (Single ("\"unterminated").Type, JSONTokenError);
	CHECK_EQ (Single ("\"raw\ncontrol\"").Type, JSONTokenError);
}

TEST (TokenizerUnsignedIntegers)
{
	TJSONToken Token = Single ("38000");
	CHECK_EQ (Token.Type, JSONTokenNumber);
	CHECK (Token.bIsUnsigned);
	CHECK_EQ (Token.nValue, 38000u);

	Token = Single ("0");
	CHECK (Token.bIsUnsigned);
	CHECK_EQ (Token.nValue, 0u);

	Token = Single ("4294967295");
	CHECK (Token.bIsUnsigned);
	CHECK_EQ (Token.nValue, 4294967295u);
}

TEST (TokenizerNonUnsignedNumbers)
{
	CHECK (!Single ("4294967296").bIsUnsigned);
	CHECK (!Single ("-5").bIsUnsigned);
	CHECK (!Single ("560.0").bIsUnsigned);
	CHECK (!Single ("1e3").bIsUnsigned);
	CHECK_EQ (Single ("-1.5E-3").Type, JSONTokenNumber);
	CHECK_EQ (Single ("99999999999999999999").Type, JSONTokenNumber);
}

TEST (TokenizerInvalidNumbers)
{
	CHECK_EQ (Single ("012").Type, JSONTokenError);
	CHECK_EQ (Single ("-").Type, JSONTokenError);
	CHECK_EQ (Single ("1.").Type, JSONTokenError);
	CHECK_EQ (Single ("1e").Type, JSONTokenError);
	CHECK_EQ (Single ("+1").Type, JSONTokenError);
}

TEST (TokenizerStopsAtBufferEnd)
{
	// The buffer is not NUL-terminated: only the first 6 bytes belong to it.
	const char Text[] = "{\"raw\"XYZ";
	CJSONTokenizer Tokenizer (Text, 6);
	TJSONToken Token;

	CHECK_EQ (Tokenizer.Next (&Token), JSONTokenObjectBegin);
	CHECK_EQ (Tokenizer.Next (&Token), JSONTokenString);
	CHECK_EQ (Tokenizer.Next (&Token), JSONTokenEnd);
}

TEST (TokenizerErrorRepeats)
{
	CJSONTokenizer Tokenizer ("@", 1);
	TJSONToken Token;

	CHECK_EQ (Tokenizer.Next (&Token), JSONTokenError);
	CHECK_EQ (Tokenizer.Next (&Token), JSONTokenError);
}
