/*
 *  jsontokenizer.cpp
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

#include "jsontokenizer.h"

namespace
{
	bool IsDigit (char ch)
	{
		return ch >= '0' && ch <= '9';
	}

	bool IsHexDigit (char ch)
	{
		return IsDigit (ch) || (ch >= 'a' && ch <= 'f') || (ch >= 'A' && ch <= 'F');
	}
}

CJSONTokenizer::CJSONTokenizer (const char *pText, size_t nLength)
:	m_pText (pText),
	m_nLength (nLength),
	m_nPos (0)
{
}

TJSONTokenType CJSONTokenizer::Next (TJSONToken *pToken)
{
	while (   m_nPos < m_nLength
	       && (   m_pText[m_nPos] == ' ' || m_pText[m_nPos] == '\t'
		   || m_pText[m_nPos] == '\r' || m_pText[m_nPos] == '\n'))
	{
		m_nPos++;
	}

	pToken->pText = m_pText + m_nPos;
	pToken->nLength = 1;
	pToken->bIsUnsigned = false;
	pToken->nValue = 0;

	if (m_nPos >= m_nLength)
	{
		pToken->nLength = 0;
		return pToken->Type = JSONTokenEnd;
	}

	char ch = m_pText[m_nPos];
	switch (ch)
	{
	case '{':	m_nPos++;	return pToken->Type = JSONTokenObjectBegin;
	case '}':	m_nPos++;	return pToken->Type = JSONTokenObjectEnd;
	case '[':	m_nPos++;	return pToken->Type = JSONTokenArrayBegin;
	case ']':	m_nPos++;	return pToken->Type = JSONTokenArrayEnd;
	case ':':	m_nPos++;	return pToken->Type = JSONTokenColon;
	case ',':	m_nPos++;	return pToken->Type = JSONTokenComma;

	case '"':
		return pToken->Type = ScanString (pToken) ? JSONTokenString : JSONTokenError;

	case 't':
		return pToken->Type = ScanLiteral ("true") ? JSONTokenTrue : JSONTokenError;

	case 'f':
		return pToken->Type = ScanLiteral ("false") ? JSONTokenFalse : JSONTokenError;

	case 'n':
		return pToken->Type = ScanLiteral ("null") ? JSONTokenNull : JSONTokenError;

	default:
		if (ch == '-' || IsDigit (ch))
		{
			return pToken->Type = ScanNumber (pToken) ? JSONTokenNumber : JSONTokenError;
		}
		break;
	}

	// Stay on the error position, so further calls keep returning an error.
	return pToken->Type = JSONTokenError;
}

bool CJSONTokenizer::ScanString (TJSONToken *pToken)
{
	size_t nPos = m_nPos + 1;		// skip opening quote

	while (nPos < m_nLength)
	{
		char ch = m_pText[nPos];

		if (ch == '"')
		{
			pToken->pText = m_pText + m_nPos + 1;
			pToken->nLength = nPos - m_nPos - 1;
			m_nPos = nPos + 1;
			return true;
		}

		if ((unsigned char) ch < 0x20)
		{
			return false;		// control characters must be escaped
		}

		if (ch == '\\')
		{
			if (++nPos >= m_nLength)
			{
				return false;
			}

			switch (m_pText[nPos])
			{
			case '"': case '\\': case '/': case 'b':
			case 'f': case 'n':  case 'r': case 't':
				break;

			case 'u':
				for (unsigned i = 0; i < 4; i++)
				{
					if (++nPos >= m_nLength || !IsHexDigit (m_pText[nPos]))
					{
						return false;
					}
				}
				break;

			default:
				return false;
			}
		}

		nPos++;
	}

	return false;				// unterminated
}

bool CJSONTokenizer::ScanNumber (TJSONToken *pToken)
{
	size_t nPos = m_nPos;
	bool bNegative = false;
	bool bInteger = true;
	bool bFits = true;
	uint32_t nValue = 0;

	if (m_pText[nPos] == '-')
	{
		bNegative = true;
		nPos++;
	}

	// int: 0 | [1-9][0-9]*
	if (nPos >= m_nLength || !IsDigit (m_pText[nPos]))
	{
		return false;
	}

	if (m_pText[nPos] == '0')
	{
		nPos++;
		if (nPos < m_nLength && IsDigit (m_pText[nPos]))
		{
			return false;		// leading zeros are not allowed
		}
	}
	else
	{
		while (nPos < m_nLength && IsDigit (m_pText[nPos]))
		{
			uint32_t nDigit = (uint32_t) (m_pText[nPos] - '0');
			if (nValue > (0xFFFFFFFFu - nDigit) / 10)
			{
				bFits = false;
			}
			else
			{
				nValue = nValue * 10 + nDigit;
			}
			nPos++;
		}
	}

	// frac: . [0-9]+
	if (nPos < m_nLength && m_pText[nPos] == '.')
	{
		bInteger = false;
		nPos++;
		if (nPos >= m_nLength || !IsDigit (m_pText[nPos]))
		{
			return false;
		}
		while (nPos < m_nLength && IsDigit (m_pText[nPos]))
		{
			nPos++;
		}
	}

	// exp: [eE] [+-]? [0-9]+
	if (nPos < m_nLength && (m_pText[nPos] == 'e' || m_pText[nPos] == 'E'))
	{
		bInteger = false;
		nPos++;
		if (nPos < m_nLength && (m_pText[nPos] == '+' || m_pText[nPos] == '-'))
		{
			nPos++;
		}
		if (nPos >= m_nLength || !IsDigit (m_pText[nPos]))
		{
			return false;
		}
		while (nPos < m_nLength && IsDigit (m_pText[nPos]))
		{
			nPos++;
		}
	}

	pToken->pText = m_pText + m_nPos;
	pToken->nLength = nPos - m_nPos;
	pToken->bIsUnsigned = bInteger && bFits && (!bNegative || nValue == 0);
	pToken->nValue = pToken->bIsUnsigned ? nValue : 0;
	m_nPos = nPos;

	return true;
}

bool CJSONTokenizer::ScanLiteral (const char *pLiteral)
{
	size_t nPos = m_nPos;
	for (; *pLiteral != '\0'; pLiteral++, nPos++)
	{
		if (nPos >= m_nLength || m_pText[nPos] != *pLiteral)
		{
			return false;
		}
	}

	m_nPos = nPos;
	return true;
}
