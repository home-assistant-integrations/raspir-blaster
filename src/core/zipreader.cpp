/*
 *  zipreader.cpp
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

#include "zipreader.h"
#include "inflate.h"
#include "textwriter.h"

// ----- ZIP signatures -----------------------------------------------------------

#define SIG_EOCD	0x06054B50u	// End of Central Directory
#define SIG_CD		0x02014B50u	// Central Directory entry
#define SIG_LOCAL	0x04034B50u	// Local File Header

// Compression methods
#define ZIP_STORED	0
#define ZIP_DEFLATED	8

// ----- helpers ------------------------------------------------------------------

static uint16_t U16LE (const uint8_t *p)
{
	return (uint16_t) (p[0] | ((unsigned) p[1] << 8));
}

static uint32_t U32LE (const uint8_t *p)
{
	return (uint32_t) p[0]
	     | ((uint32_t) p[1] <<  8)
	     | ((uint32_t) p[2] << 16)
	     | ((uint32_t) p[3] << 24);
}

// CRC-32 (ISO 3309 / PKZIP polynomial 0xEDB88320, reflected)
static uint32_t CRC32 (const uint8_t *pData, size_t nLen)
{
	uint32_t nCRC = 0xFFFFFFFFu;
	for (size_t i = 0; i < nLen; i++)
	{
		nCRC ^= pData[i];
		for (int j = 0; j < 8; j++)
			nCRC = (nCRC >> 1) ^ (0xEDB88320u & ~((nCRC & 1u) - 1u));
	}
	return nCRC ^ 0xFFFFFFFFu;
}

// Return true if pData[0..nDataLen-1] equals the NUL-terminated string pStr.
static bool NameMatch (const uint8_t *pData, size_t nDataLen, const char *pStr)
{
	size_t nStrLen = CoreStrLen (pStr);
	if (nDataLen != nStrLen)
		return false;
	for (size_t i = 0; i < nDataLen; i++)
	{
		if (pData[i] != (uint8_t) pStr[i])
			return false;
	}
	return true;
}

// Bounds-checked access: return false if [nOffset, nOffset+nSize) goes out of range.
static bool InBounds (size_t nZipLen, size_t nOffset, size_t nSize)
{
	return nOffset <= nZipLen && nSize <= nZipLen - nOffset;
}

// ----- EOCD search and central directory scan -----------------------------------

TZipResult ZipExtract (const uint8_t *pZip, size_t nZipLen,
                       const char *pName,
                       uint8_t *pOutput, size_t nOutputMax,
                       size_t *pOutputLen)
{
	// --- Locate End of Central Directory (scan backwards, up to 65535-byte comment) ---
	if (nZipLen < 22)
		return ZipInvalidFormat;

	size_t nEOCD = nZipLen;
	bool   bFound = false;
	size_t nScanLimit = nZipLen < (65535u + 22u) ? 0u : nZipLen - (65535u + 22u);

	// Scan from end; must be at least 22 bytes from the end
	size_t nPos = nZipLen - 22u;
	while (true)
	{
		if (InBounds (nZipLen, nPos, 4) && U32LE (pZip + nPos) == SIG_EOCD)
		{
			// Verify: comment length matches actual remaining bytes
			uint16_t nCommentLen = U16LE (pZip + nPos + 20);
			if (nPos + 22u + nCommentLen == nZipLen)
			{
				nEOCD  = nPos;
				bFound = true;
				break;
			}
		}
		if (nPos == nScanLimit)
			break;
		nPos--;
	}

	if (!bFound)
		return ZipInvalidFormat;

	// Read EOCD fields
	uint16_t nTotalEntries = U16LE (pZip + nEOCD + 10);
	uint32_t nCDOffset     = U32LE (pZip + nEOCD + 16);
	uint32_t nCDSize       = U32LE (pZip + nEOCD + 12);

	// Reject ZIP64 placeholders
	if (nCDOffset == 0xFFFFFFFFu || nCDSize == 0xFFFFFFFFu)
		return ZipUnsupported;

	if (!InBounds (nZipLen, nCDOffset, nCDSize))
		return ZipInvalidFormat;

	// --- Scan Central Directory for the requested entry ---
	size_t nCDPos = (size_t) nCDOffset;
	for (uint16_t nEntry = 0; nEntry < nTotalEntries; nEntry++)
	{
		if (!InBounds (nZipLen, nCDPos, 46u))
			return ZipInvalidFormat;
		if (U32LE (pZip + nCDPos) != SIG_CD)
			return ZipInvalidFormat;

		uint16_t nMethod      = U16LE (pZip + nCDPos + 10);
		uint32_t nStoredCRC   = U32LE (pZip + nCDPos + 16);
		uint32_t nCompSize    = U32LE (pZip + nCDPos + 20);
		uint32_t nUncompSize  = U32LE (pZip + nCDPos + 24);
		uint16_t nNameLen     = U16LE (pZip + nCDPos + 28);
		uint16_t nExtraLen    = U16LE (pZip + nCDPos + 30);
		uint16_t nCommentLen  = U16LE (pZip + nCDPos + 32);
		uint32_t nLocalOffset = U32LE (pZip + nCDPos + 42);

		// Reject ZIP64 entries
		if (nCompSize == 0xFFFFFFFFu || nUncompSize == 0xFFFFFFFFu)
			return ZipUnsupported;

		size_t nNameOffset = nCDPos + 46u;
		if (!InBounds (nZipLen, nNameOffset, nNameLen))
			return ZipInvalidFormat;

		bool bMatch = NameMatch (pZip + nNameOffset, nNameLen, pName);

		// Advance past this CD entry
		nCDPos += 46u + nNameLen + nExtraLen + nCommentLen;

		if (!bMatch)
			continue;

		// --- Found the entry — read its local file header to find data offset ---
		if (!InBounds (nZipLen, nLocalOffset, 30u))
			return ZipInvalidFormat;
		if (U32LE (pZip + nLocalOffset) != SIG_LOCAL)
			return ZipInvalidFormat;

		uint16_t nLocalNameLen  = U16LE (pZip + nLocalOffset + 26);
		uint16_t nLocalExtraLen = U16LE (pZip + nLocalOffset + 28);
		size_t   nDataOffset    = (size_t) nLocalOffset + 30u
		                        + nLocalNameLen + nLocalExtraLen;

		if (!InBounds (nZipLen, nDataOffset, (size_t) nCompSize))
			return ZipInvalidFormat;

		// --- Extract ---
		if (nMethod == ZIP_STORED)
		{
			// Stored (uncompressed) data: the two sizes must match, or a
			// corrupted/malicious entry could claim an uncompressed size
			// larger than what was bounds-checked above and read past it.
			if (nUncompSize != nCompSize)
				return ZipInvalidFormat;
			if (nUncompSize > nOutputMax)
				return ZipOutputFull;
			const uint8_t *pSrc = pZip + nDataOffset;
			for (size_t i = 0; i < (size_t) nUncompSize; i++)
				pOutput[i] = pSrc[i];
			*pOutputLen = (size_t) nUncompSize;
		}
		else if (nMethod == ZIP_DEFLATED)
		{
			size_t nOut = 0;
			TInflateResult nRes = Inflate (pZip + nDataOffset, (size_t) nCompSize,
			                               pOutput, nOutputMax, &nOut);
			if (nRes == InflateOutputFull)
				return ZipOutputFull;
			if (nRes != InflateOK)
				return ZipInflateError;
			if (nOut != (size_t) nUncompSize)
				return ZipInflateError;
			*pOutputLen = nOut;
		}
		else
		{
			return ZipUnsupported;
		}

		// --- Verify CRC-32 ---
		uint32_t nActualCRC = CRC32 (pOutput, *pOutputLen);
		if (nActualCRC != nStoredCRC)
			return ZipCRCError;

		return ZipOK;
	}

	return ZipNotFound;
}
