/*
 *  wpareader.cpp
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

#include "wpareader.h"

namespace
{
	struct TSlice
	{
		const char *p;
		size_t      n;
	};

	bool IsSpace (char ch)
	{
		return ch == ' ' || ch == '\t' || ch == '\r';
	}

	TSlice Trim (TSlice s)
	{
		while (s.n > 0 && IsSpace (s.p[0]))
		{
			s.p++;
			s.n--;
		}
		while (s.n > 0 && IsSpace (s.p[s.n - 1]))
		{
			s.n--;
		}
		return s;
	}

	bool SliceStartsWith (TSlice s, const char *pPrefix)
	{
		size_t i = 0;
		while (pPrefix[i] != '\0')
		{
			if (i >= s.n || s.p[i] != pPrefix[i])
			{
				return false;
			}
			i++;
		}
		return true;
	}

	// Copies the value of key="value" (the part between the quotes) into
	// pDest (size nMax incl. NUL). Returns false if not found or too long.
	bool ExtractQuoted (TSlice s, const char *pKey, char *pDest, size_t nMax)
	{
		if (!SliceStartsWith (s, pKey))
		{
			return false;
		}

		size_t i = 0;
		while (pKey[i] != '\0')
		{
			i++;
		}

		if (i >= s.n || s.p[i] != '"')
		{
			return false;
		}
		i++;

		size_t nStart = i;
		while (i < s.n && s.p[i] != '"')
		{
			i++;
		}
		if (i >= s.n)
		{
			return false;	// no closing quote
		}

		size_t nLen = i - nStart;
		if (nLen >= nMax)
		{
			return false;
		}

		for (size_t j = 0; j < nLen; j++)
		{
			pDest[j] = s.p[nStart + j];
		}
		pDest[nLen] = '\0';

		return true;
	}
}

void WPAConfigRead (const char *pText, size_t nLength, TWiFiStatus *pStatus)
{
	pStatus->bConfigured = false;
	pStatus->bPasswordSet = false;
	pStatus->SSID[0] = '\0';
	pStatus->Country[0] = '\0';

	bool bInNetworkBlock = false;

	size_t nPos = 0;
	while (nPos < nLength)
	{
		TSlice Line = { pText + nPos, 0 };
		while (nPos < nLength && pText[nPos] != '\n')
		{
			nPos++;
			Line.n++;
		}
		nPos++;	// skip '\n'

		Line = Trim (Line);
		if (Line.n == 0)
		{
			continue;
		}

		if (!bInNetworkBlock && SliceStartsWith (Line, "network={"))
		{
			bInNetworkBlock = true;
			continue;
		}

		if (bInNetworkBlock && Line.n == 1 && Line.p[0] == '}')
		{
			bInNetworkBlock = false;
			continue;
		}

		if (bInNetworkBlock)
		{
			if (ExtractQuoted (Line, "ssid=", pStatus->SSID, sizeof pStatus->SSID))
			{
				pStatus->bConfigured = true;
			}
			else if (SliceStartsWith (Line, "psk="))
			{
				pStatus->bPasswordSet = true;
			}
			continue;
		}

		if (SliceStartsWith (Line, "country=") && Line.n == 8 + 2)
		{
			pStatus->Country[0] = Line.p[8];
			pStatus->Country[1] = Line.p[9];
			pStatus->Country[2] = '\0';
		}
	}
}
