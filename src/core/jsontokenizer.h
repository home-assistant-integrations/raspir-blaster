/*
 *  jsontokenizer.h
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

#ifndef _raspir_jsontokenizer_h
#define _raspir_jsontokenizer_h

#include <stddef.h>
#include <stdint.h>

enum TJSONTokenType
{
	JSONTokenObjectBegin,	// {
	JSONTokenObjectEnd,	// }
	JSONTokenArrayBegin,	// [
	JSONTokenArrayEnd,	// ]
	JSONTokenColon,		// :
	JSONTokenComma,		// ,
	JSONTokenString,	// "..." (pText/nLength: raw contents between the quotes)
	JSONTokenNumber,	// any JSON number (see bIsUnsigned / nValue)
	JSONTokenTrue,
	JSONTokenFalse,
	JSONTokenNull,
	JSONTokenEnd,		// end of input
	JSONTokenError		// invalid input
};

struct TJSONToken
{
	TJSONTokenType Type;
	const char    *pText;		// start of the token in the input
	size_t         nLength;		// length (for strings: contents without quotes)
	bool           bIsUnsigned;	// number is a non-negative integer that fits in 32 bit
	uint32_t       nValue;		// value if bIsUnsigned
};

/// Zero-allocation JSON tokenizer working on a non-terminated buffer.
/// Strings are validated (escape sequences, no control characters) but not
/// decoded.
class CJSONTokenizer
{
public:
	CJSONTokenizer (const char *pText, size_t nLength);

	/// \return the type of the next token; JSONTokenEnd/JSONTokenError repeat
	TJSONTokenType Next (TJSONToken *pToken);

private:
	bool ScanString (TJSONToken *pToken);
	bool ScanNumber (TJSONToken *pToken);
	bool ScanLiteral (const char *pLiteral);

private:
	const char *m_pText;
	size_t      m_nLength;
	size_t      m_nPos;
};

#endif
