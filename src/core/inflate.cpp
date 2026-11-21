/*
 *  inflate.cpp
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

#include "inflate.h"

// ----- constants ----------------------------------------------------------------

#define DEFLATE_MAX_BITS	15
#define DEFLATE_MAX_LITLEN	288
#define DEFLATE_MAX_DIST	32
#define DEFLATE_MAX_CODES	(DEFLATE_MAX_LITLEN + DEFLATE_MAX_DIST)
#define DEFLATE_CL_CODES	19

// Fixed literal/length code lengths: 0-143 → 8, 144-255 → 9, 256-279 → 7, 280-287 → 8
static const uint8_t s_FixedLitLen[288] = {
	8,8,8,8,8,8,8,8, 8,8,8,8,8,8,8,8, 8,8,8,8,8,8,8,8, 8,8,8,8,8,8,8,8,
	8,8,8,8,8,8,8,8, 8,8,8,8,8,8,8,8, 8,8,8,8,8,8,8,8, 8,8,8,8,8,8,8,8,
	8,8,8,8,8,8,8,8, 8,8,8,8,8,8,8,8, 8,8,8,8,8,8,8,8, 8,8,8,8,8,8,8,8,
	8,8,8,8,8,8,8,8, 8,8,8,8,8,8,8,8, 8,8,8,8,8,8,8,8, 8,8,8,8,8,8,8,8,
	8,8,8,8,8,8,8,8, 8,8,8,8,8,8,8,8,  // 128-143
	9,9,9,9,9,9,9,9, 9,9,9,9,9,9,9,9, 9,9,9,9,9,9,9,9, 9,9,9,9,9,9,9,9,
	9,9,9,9,9,9,9,9, 9,9,9,9,9,9,9,9, 9,9,9,9,9,9,9,9, 9,9,9,9,9,9,9,9,
	9,9,9,9,9,9,9,9, 9,9,9,9,9,9,9,9, 9,9,9,9,9,9,9,9, 9,9,9,9,9,9,9,9,
	9,9,9,9,9,9,9,9, 9,9,9,9,9,9,9,9,  // 240-255
	7,7,7,7,7,7,7,7, 7,7,7,7,7,7,7,7, 7,7,7,7,7,7,7,7,  // 256-279
	8,8,8,8,8,8,8,8  // 280-287
};

static const uint8_t s_FixedDist[32] = {
	5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5
};

// Order for reading HCLEN code lengths in dynamic blocks (RFC 1951 §3.2.7)
static const uint8_t s_ClcOrder[19] = {
	16,17,18,0,8,7,9,6,10,5,11,4,12,3,13,2,14,1,15
};

// Length symbol (257-285) extra bits and base lengths (RFC 1951 Table 3)
static const uint8_t  s_LenExtra[29] = {
	0,0,0,0,0,0,0,0,1,1,1,1,2,2,2,2,3,3,3,3,4,4,4,4,5,5,5,5,0
};
static const uint16_t s_LenBase[29] = {
	3,4,5,6,7,8,9,10,11,13,15,17,19,23,27,31,
	35,43,51,59,67,83,99,115,131,163,195,227,258
};

// Distance symbol (0-29) extra bits and base distances (RFC 1951 Table 4)
static const uint8_t  s_DistExtra[30] = {
	0,0,0,0,1,1,2,2,3,3,4,4,5,5,6,6,7,7,8,8,9,9,10,10,11,11,12,12,13,13
};
static const uint16_t s_DistBase[30] = {
	1,2,3,4,5,7,9,13,17,25,33,49,65,97,129,193,
	257,385,513,769,1025,1537,2049,3073,4097,6145,
	8193,12289,16385,24577
};

// ----- bit reader ---------------------------------------------------------------

struct SBitReader
{
	const uint8_t	*m_pData;
	size_t		 m_nLen;
	size_t		 m_nBytePos;
	uint32_t	 m_nBuf;	// bit buffer, LSB-first packed
	unsigned	 m_nBitsLeft;	// valid bits in m_nBuf

	void Init (const uint8_t *pData, size_t nLen)
	{
		m_pData = pData;
		m_nLen = nLen;
		m_nBytePos = 0;
		m_nBuf = 0;
		m_nBitsLeft = 0;
	}

	// Fill buffer with at least n bits (n <= 24)
	bool Fill (unsigned n)
	{
		while (m_nBitsLeft < n)
		{
			if (m_nBytePos >= m_nLen)
				return false;
			m_nBuf |= (uint32_t) m_pData[m_nBytePos++] << m_nBitsLeft;
			m_nBitsLeft += 8;
		}
		return true;
	}

	// Read n bits LSB-first (for lengths, extra bits, non-Huffman values)
	bool Read (unsigned n, unsigned *pVal)
	{
		if (!Fill (n))
			return false;
		*pVal = m_nBuf & ((1u << n) - 1u);
		m_nBuf >>= n;
		m_nBitsLeft -= n;
		return true;
	}

	// Read one bit for Huffman decode; returns 0 or 1, or -1 on truncation
	int ReadBit (void)
	{
		if (m_nBitsLeft == 0)
		{
			if (m_nBytePos >= m_nLen)
				return -1;
			m_nBuf = m_pData[m_nBytePos++];
			m_nBitsLeft = 8;
		}
		int nBit = (int) (m_nBuf & 1u);
		m_nBuf >>= 1;
		m_nBitsLeft--;
		return nBit;
	}

	// Discard remaining bits in the current byte (align to byte boundary)
	void AlignByte (void)
	{
		unsigned nSkip = m_nBitsLeft & 7u;
		m_nBuf >>= nSkip;
		m_nBitsLeft -= nSkip;
	}
};

// ----- canonical Huffman table --------------------------------------------------

struct SHuffman
{
	uint16_t m_Count[DEFLATE_MAX_BITS + 1];	// m_Count[i]: number of codes with length i
	uint16_t m_Symbol[DEFLATE_MAX_CODES];	// symbols sorted by (length, symbol value)

	// Build from code-length array. Lengths of 0 are ignored (no code assigned).
	bool Build (const uint8_t *pLengths, unsigned nSymbols)
	{
		unsigned i;
		for (i = 0; i <= DEFLATE_MAX_BITS; i++)
			m_Count[i] = 0;
		for (i = 0; i < nSymbols; i++)
		{
			if (pLengths[i] > DEFLATE_MAX_BITS)
				return false;
			m_Count[pLengths[i]]++;
		}
		m_Count[0] = 0;

		// Bucket-sort symbols by code length
		uint16_t offsets[DEFLATE_MAX_BITS + 1];
		offsets[0] = 0;
		for (i = 1; i <= DEFLATE_MAX_BITS; i++)
			offsets[i] = (uint16_t) (offsets[i - 1] + m_Count[i - 1]);

		for (i = 0; i < nSymbols; i++)
		{
			if (pLengths[i] != 0)
				m_Symbol[offsets[pLengths[i]]++] = (uint16_t) i;
		}
		return true;
	}

	// Decode one symbol using the puff.c canonical algorithm (O(max_bits) per symbol).
	// Bits are accumulated MSB-first by reading one stream bit at a time.
	// Returns the symbol value or -1 on truncated input or invalid code.
	int Decode (SBitReader *pBits) const
	{
		int code = 0, first = 0, index = 0;
		for (int nLen = 1; nLen <= DEFLATE_MAX_BITS; nLen++)
		{
			int nBit = pBits->ReadBit ();
			if (nBit < 0)
				return -1;
			code |= nBit;
			int nCount = m_Count[nLen];
			if (code - nCount < first)
				return m_Symbol[index + (code - first)];
			index += nCount;
			first += nCount;
			first <<= 1;
			code  <<= 1;
		}
		return -1;
	}
};

// ----- output buffer ------------------------------------------------------------

struct SOutput
{
	uint8_t	*m_pBuf;
	size_t	 m_nMax;
	size_t	 m_nPos;
	bool	 m_bFull;

	void Init (uint8_t *pBuf, size_t nMax)
	{
		m_pBuf = pBuf;
		m_nMax = nMax;
		m_nPos = 0;
		m_bFull = false;
	}

	bool WriteByte (uint8_t nByte)
	{
		if (m_nPos >= m_nMax)
		{
			m_bFull = true;
			return false;
		}
		m_pBuf[m_nPos++] = nByte;
		return true;
	}

	// Copy a back-reference match; dist may be < len (run-length encoding)
	bool CopyMatch (unsigned nDist, unsigned nLen)
	{
		if (nDist > m_nPos || nDist == 0)
			return false;
		for (unsigned i = 0; i < nLen; i++)
		{
			if (!WriteByte (m_pBuf[m_nPos - nDist]))
				return false;
		}
		return true;
	}
};

// ----- block decoding -----------------------------------------------------------

static TInflateResult DecodeBlock (SBitReader *pBits, SOutput *pOut,
                                   const SHuffman *pLitLen,
                                   const SHuffman *pDist)
{
	for (;;)
	{
		int nSym = pLitLen->Decode (pBits);
		if (nSym < 0)
			return InflateTruncated;

		if (nSym < 256)
		{
			if (!pOut->WriteByte ((uint8_t) nSym))
				return InflateOutputFull;
		}
		else if (nSym == 256)
		{
			return InflateOK;
		}
		else
		{
			// Length code (257-285)
			if (nSym > 285)
				return InflateInvalidInput;

			unsigned nExtra, nLen;
			nLen = s_LenBase[nSym - 257];
			if (s_LenExtra[nSym - 257] > 0)
			{
				if (!pBits->Read (s_LenExtra[nSym - 257], &nExtra))
					return InflateTruncated;
				nLen += nExtra;
			}

			int nDSym = pDist->Decode (pBits);
			if (nDSym < 0)
				return InflateTruncated;
			if (nDSym > 29)
				return InflateInvalidInput;

			unsigned nDist = s_DistBase[nDSym];
			if (s_DistExtra[nDSym] > 0)
			{
				if (!pBits->Read (s_DistExtra[nDSym], &nExtra))
					return InflateTruncated;
				nDist += nExtra;
			}

			if (!pOut->CopyMatch (nDist, nLen))
			{
				if (pOut->m_bFull)
					return InflateOutputFull;
				return InflateInvalidInput;
			}
		}
	}
}

// Build literal/length and distance Huffman tables from a dynamic block header.
static TInflateResult BuildDynamic (SBitReader *pBits,
                                    SHuffman *pLitLen, SHuffman *pDist)
{
	unsigned nHLit, nHDist, nHCLen;
	if (!pBits->Read (5, &nHLit) || !pBits->Read (5, &nHDist) || !pBits->Read (4, &nHCLen))
		return InflateTruncated;
	nHLit  += 257;
	nHDist += 1;
	nHCLen += 4;
	if (nHLit > DEFLATE_MAX_LITLEN || nHDist > DEFLATE_MAX_DIST)
		return InflateInvalidInput;

	// Read the code-length code lengths (3 bits each, in a scrambled order)
	uint8_t aClcLen[DEFLATE_CL_CODES];
	for (unsigned i = 0; i < DEFLATE_CL_CODES; i++)
		aClcLen[i] = 0;
	for (unsigned i = 0; i < nHCLen; i++)
	{
		unsigned nV;
		if (!pBits->Read (3, &nV))
			return InflateTruncated;
		aClcLen[s_ClcOrder[i]] = (uint8_t) nV;
	}

	SHuffman clc;
	if (!clc.Build (aClcLen, DEFLATE_CL_CODES))
		return InflateInvalidInput;

	// Decode the actual code lengths for the lit/len and dist alphabets
	uint8_t aLengths[DEFLATE_MAX_CODES];
	for (unsigned i = 0; i < DEFLATE_MAX_CODES; i++)
		aLengths[i] = 0;

	unsigned nTotal = nHLit + nHDist;
	unsigned nPrev  = 0;
	for (unsigned i = 0; i < nTotal; )
	{
		int nSym = clc.Decode (pBits);
		if (nSym < 0)
			return InflateTruncated;

		if (nSym < 16)
		{
			aLengths[i++] = (uint8_t) nSym;
			nPrev = (unsigned) nSym;
		}
		else if (nSym == 16)
		{
			unsigned nRep;
			if (!pBits->Read (2, &nRep))
				return InflateTruncated;
			nRep += 3;
			if (i + nRep > nTotal)
				return InflateInvalidInput;
			for (unsigned j = 0; j < nRep; j++)
				aLengths[i++] = (uint8_t) nPrev;
		}
		else if (nSym == 17)
		{
			unsigned nRep;
			if (!pBits->Read (3, &nRep))
				return InflateTruncated;
			nRep += 3;
			if (i + nRep > nTotal)
				return InflateInvalidInput;
			for (unsigned j = 0; j < nRep; j++)
				aLengths[i++] = 0;
			nPrev = 0;
		}
		else  // nSym == 18
		{
			unsigned nRep;
			if (!pBits->Read (7, &nRep))
				return InflateTruncated;
			nRep += 11;
			if (i + nRep > nTotal)
				return InflateInvalidInput;
			for (unsigned j = 0; j < nRep; j++)
				aLengths[i++] = 0;
			nPrev = 0;
		}
	}

	if (!pLitLen->Build (aLengths, nHLit))
		return InflateInvalidInput;
	if (!pDist->Build (aLengths + nHLit, nHDist))
		return InflateInvalidInput;

	return InflateOK;
}

// ----- public API ---------------------------------------------------------------

TInflateResult Inflate (const uint8_t *pInput, size_t nInputLen,
                        uint8_t *pOutput, size_t nOutputMax,
                        size_t *pOutputLen)
{
	SBitReader bits;
	bits.Init (pInput, nInputLen);

	SOutput out;
	out.Init (pOutput, nOutputMax);

	bool bFinal = false;
	while (!bFinal)
	{
		unsigned nBFinal, nBType;
		if (!bits.Read (1, &nBFinal) || !bits.Read (2, &nBType))
			return InflateTruncated;
		bFinal = (nBFinal != 0);

		if (nBType == 0)
		{
			// Stored block: skip to byte boundary, then LEN + NLEN + data
			bits.AlignByte ();
			unsigned nLo, nHi;
			if (!bits.Read (8, &nLo) || !bits.Read (8, &nHi))
				return InflateTruncated;
			unsigned nLen = nLo | (nHi << 8);
			if (!bits.Read (8, &nLo) || !bits.Read (8, &nHi))
				return InflateTruncated;
			unsigned nNLen = nLo | (nHi << 8);
			if ((nLen ^ nNLen) != 0xFFFFu)
				return InflateInvalidInput;
			for (unsigned i = 0; i < nLen; i++)
			{
				unsigned nByte;
				if (!bits.Read (8, &nByte))
					return InflateTruncated;
				if (!out.WriteByte ((uint8_t) nByte))
					return InflateOutputFull;
			}
		}
		else if (nBType == 1)
		{
			// Fixed Huffman codes
			SHuffman litLen, dist;
			litLen.Build (s_FixedLitLen, DEFLATE_MAX_LITLEN);
			dist.Build (s_FixedDist, DEFLATE_MAX_DIST);
			TInflateResult nRes = DecodeBlock (&bits, &out, &litLen, &dist);
			if (nRes != InflateOK)
				return nRes;
		}
		else if (nBType == 2)
		{
			// Dynamic Huffman codes
			SHuffman litLen, dist;
			TInflateResult nRes = BuildDynamic (&bits, &litLen, &dist);
			if (nRes != InflateOK)
				return nRes;
			nRes = DecodeBlock (&bits, &out, &litLen, &dist);
			if (nRes != InflateOK)
				return nRes;
		}
		else
		{
			return InflateInvalidInput;	// reserved block type
		}
	}

	*pOutputLen = out.m_nPos;
	return InflateOK;
}
