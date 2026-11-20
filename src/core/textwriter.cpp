/*
 *  textwriter.cpp
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

#include "textwriter.h"

size_t CoreStrLen (const char *pString)
{
	size_t nLength = 0;
	while (pString[nLength] != '\0')
	{
		nLength++;
	}

	return nLength;
}

bool CoreStrEqual (const char *pString1, const char *pString2)
{
	while (*pString1 != '\0' && *pString1 == *pString2)
	{
		pString1++;
		pString2++;
	}

	return *pString1 == *pString2;
}

bool CoreStrEqualN (const char *pString, size_t nLength, const char *pCString)
{
	for (size_t i = 0; i < nLength; i++)
	{
		if (pCString[i] == '\0' || pCString[i] != pString[i])
		{
			return false;
		}
	}

	return pCString[nLength] == '\0';
}

CTextWriter::CTextWriter (char *pBuffer, size_t nSize)
:	m_pBuffer (pBuffer),
	m_nSize (nSize),
	m_nLength (0),
	m_bOverflow (nSize == 0)
{
	if (nSize > 0)
	{
		m_pBuffer[0] = '\0';
	}
}

void CTextWriter::AppendChar (char chChar)
{
	if (m_nLength + 1 >= m_nSize)
	{
		m_bOverflow = true;
		return;
	}

	m_pBuffer[m_nLength++] = chChar;
	m_pBuffer[m_nLength] = '\0';
}

void CTextWriter::Append (const char *pString, size_t nLength)
{
	for (size_t i = 0; i < nLength && !m_bOverflow; i++)
	{
		AppendChar (pString[i]);
	}
}

void CTextWriter::Append (const char *pString)
{
	Append (pString, CoreStrLen (pString));
}

void CTextWriter::AppendUnsigned (uint32_t nValue)
{
	char Digits[10];
	unsigned nDigits = 0;

	do
	{
		Digits[nDigits++] = (char) ('0' + nValue % 10);
		nValue /= 10;
	}
	while (nValue != 0);

	while (nDigits > 0)
	{
		AppendChar (Digits[--nDigits]);
	}
}

void CTextWriter::AppendJSONString (const char *pString)
{
	static const char HexDigits[] = "0123456789abcdef";

	AppendChar ('"');

	for (; *pString != '\0'; pString++)
	{
		unsigned char uchChar = (unsigned char) *pString;

		if (uchChar == '"' || uchChar == '\\')
		{
			AppendChar ('\\');
			AppendChar ((char) uchChar);
		}
		else if (uchChar < 0x20)
		{
			Append ("\\u00");
			AppendChar (HexDigits[uchChar >> 4]);
			AppendChar (HexDigits[uchChar & 0x0F]);
		}
		else
		{
			AppendChar ((char) uchChar);
		}
	}

	AppendChar ('"');
}
