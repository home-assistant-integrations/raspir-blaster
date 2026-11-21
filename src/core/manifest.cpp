/*
 *  manifest.cpp
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

#include "manifest.h"
#include "textwriter.h"

static const char SHA256Prefix[] = "sha256.";

// Copies [pStart, pStart + nLength) as a C string; false if too long or empty.
static bool CopyField (const char *pStart, size_t nLength, char *pOut, size_t nMax)
{
	if (nLength == 0 || nLength > nMax)
	{
		return false;
	}
	for (size_t i = 0; i < nLength; i++)
	{
		pOut[i] = pStart[i];
	}
	pOut[nLength] = '\0';
	return true;
}

static bool IsLowerHex (const char *p, size_t nLength)
{
	for (size_t i = 0; i < nLength; i++)
	{
		if (!((p[i] >= '0' && p[i] <= '9') || (p[i] >= 'a' && p[i] <= 'f')))
		{
			return false;
		}
	}
	return true;
}

static bool IsNameChar (char ch)
{
	return    (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z')
	       || (ch >= '0' && ch <= '9') || ch == '.' || ch == '-' || ch == '_';
}

static bool ParseLine (const char *pLine, size_t nLength, TManifest *pManifest)
{
	size_t nEq = 0;
	while (nEq < nLength && pLine[nEq] != '=')
	{
		nEq++;
	}
	if (nEq == 0 || nEq == nLength)
	{
		return false;
	}

	const char *pValue = pLine + nEq + 1;
	size_t nValue = nLength - nEq - 1;

	if (CoreStrEqualN (pLine, nEq, "version"))
	{
		return pManifest->Version[0] == '\0'
		       && CopyField (pValue, nValue, pManifest->Version, MANIFEST_VERSION_MAX);
	}

	if (CoreStrEqualN (pLine, nEq, "build"))
	{
		return pManifest->Build[0] == '\0'
		       && CopyField (pValue, nValue, pManifest->Build, MANIFEST_BUILD_MAX);
	}

	size_t nPrefix = sizeof SHA256Prefix - 1;
	if (nEq > nPrefix && CoreStrEqualN (pLine, nPrefix, SHA256Prefix))
	{
		const char *pName = pLine + nPrefix;
		size_t nName = nEq - nPrefix;
		for (size_t i = 0; i < nName; i++)
		{
			if (!IsNameChar (pName[i]))
			{
				return false;
			}
		}
		if (nValue != 64 || !IsLowerHex (pValue, nValue)
		    || pManifest->nImages >= MANIFEST_MAX_IMAGES)
		{
			return false;
		}

		TManifestImage *pImage = &pManifest->Images[pManifest->nImages];
		if (!CopyField (pName, nName, pImage->Name, MANIFEST_IMAGE_MAX))
		{
			return false;
		}
		if (ManifestFindSHA256 (*pManifest, pImage->Name) != 0)
		{
			return false;	// duplicate image
		}
		CopyField (pValue, nValue, pImage->SHA256, 64);
		pManifest->nImages++;
		return true;
	}

	return true;	// unknown keys are ignored (forward compatibility)
}

bool ManifestParse (const char *pText, size_t nLength, TManifest *pManifest)
{
	pManifest->Version[0] = '\0';
	pManifest->Build[0] = '\0';
	pManifest->nImages = 0;

	size_t i = 0;
	while (i < nLength)
	{
		size_t nStart = i;
		while (i < nLength && pText[i] != '\n')
		{
			i++;
		}
		size_t nEnd = i;
		if (nEnd > nStart && pText[nEnd - 1] == '\r')
		{
			nEnd--;
		}
		i++;	// skip '\n'

		size_t nLine = nEnd - nStart;
		if (nLine == 0 || pText[nStart] == '#')
		{
			continue;
		}
		if (!ParseLine (pText + nStart, nLine, pManifest))
		{
			return false;
		}
	}

	return pManifest->Version[0] != '\0' && pManifest->Build[0] != '\0';
}

const char *ManifestFindSHA256 (const TManifest &Manifest, const char *pImage)
{
	for (unsigned i = 0; i < Manifest.nImages; i++)
	{
		if (CoreStrEqual (Manifest.Images[i].Name, pImage))
		{
			return Manifest.Images[i].SHA256;
		}
	}
	return 0;
}

// Parses a number of nMin..nMax digits without leading zeros (except "0").
static bool ParseNumber (const char **ppText, unsigned nMin, unsigned nMax, uint32_t *pValue)
{
	const char *p = *ppText;
	unsigned nDigits = 0;
	uint32_t nValue = 0;
	while (p[nDigits] >= '0' && p[nDigits] <= '9')
	{
		if (nDigits == nMax)
		{
			return false;
		}
		nValue = nValue * 10 + (uint32_t) (p[nDigits] - '0');
		nDigits++;
	}
	if (nDigits < nMin)
	{
		return false;
	}
	*ppText = p + nDigits;
	*pValue = nValue;
	return true;
}

static bool ParseVersion (const char *p, uint32_t Fields[3])
{
	if (   !ParseNumber (&p, 4, 4, &Fields[0]) || *p++ != '.'
	    || !ParseNumber (&p, 2, 2, &Fields[1]) || *p++ != '.'
	    || !ParseNumber (&p, 1, 6, &Fields[2]) || *p != '\0')
	{
		return false;
	}

	// The release counter has no leading zeros (release-process spec).
	const char *pCounter = p;
	while (pCounter[-1] != '.')
	{
		pCounter--;
	}
	if (pCounter[0] == '0' && pCounter[1] != '\0')
	{
		return false;
	}

	return Fields[1] >= 1 && Fields[1] <= 12;
}

TVersionOrder VersionCompare (const char *pInstalled, const char *pCandidate)
{
	uint32_t Installed[3], Candidate[3];
	if (!ParseVersion (pInstalled, Installed) || !ParseVersion (pCandidate, Candidate))
	{
		return VersionNotComparable;
	}

	for (unsigned i = 0; i < 3; i++)
	{
		if (Candidate[i] != Installed[i])
		{
			return Candidate[i] > Installed[i] ? VersionNewer : VersionOlder;
		}
	}
	return VersionSame;
}
