/*
 *  formdata.cpp
 *  raspir-blaster core
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

#include "formdata.h"
#include "textwriter.h"

static int HexValue (char ch)
{
	if (ch >= '0' && ch <= '9') return ch - '0';
	if (ch >= 'a' && ch <= 'f') return ch - 'a' + 10;
	if (ch >= 'A' && ch <= 'F') return ch - 'A' + 10;
	return -1;
}

TFormResult FormDataGet (const char *pData, const char *pName, char *pValue, size_t nSize)
{
	size_t nName = CoreStrLen (pName);
	const char *p = pData;

	while (*p != '\0')
	{
		const char *pKey = p;
		while (*p != '\0' && *p != '=' && *p != '&')
		{
			p++;
		}
		size_t nKey = (size_t) (p - pKey);
		bool bMatch = nKey == nName && CoreStrEqualN (pKey, nKey, pName);

		const char *pRaw = p;
		if (*p == '=')
		{
			pRaw = ++p;
			while (*p != '\0' && *p != '&')
			{
				p++;
			}
		}
		const char *pRawEnd = p;
		if (*p == '&')
		{
			p++;
		}

		if (!bMatch)
		{
			continue;
		}

		size_t nOut = 0;
		for (const char *q = pRaw; q < pRawEnd; q++)
		{
			int nChar = *q;
			if (*q == '+')
			{
				nChar = ' ';
			}
			else if (*q == '%')
			{
				if (pRawEnd - q < 3)
				{
					return FormInvalid;
				}
				int nHigh = HexValue (q[1]), nLow = HexValue (q[2]);
				if (nHigh < 0 || nLow < 0)
				{
					return FormInvalid;
				}
				nChar = nHigh * 16 + nLow;
				q += 2;
			}

			unsigned char uch = (unsigned char) nChar;
			if (uch < 0x20 || uch == 0x7F)
			{
				return FormInvalid;
			}
			if (nOut + 1 >= nSize)
			{
				return FormTooLong;
			}
			pValue[nOut++] = (char) uch;
		}

		if (nSize == 0)
		{
			return FormTooLong;
		}
		pValue[nOut] = '\0';
		return FormFound;
	}

	return FormMissing;
}
