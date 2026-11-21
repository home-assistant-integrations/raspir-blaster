/*
 *  sha256.cpp
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

#include "sha256.h"

static const uint32_t K[64] =
{
	0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
	0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
	0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
	0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
	0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
	0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
	0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
	0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
};

static inline uint32_t Rotr (uint32_t x, unsigned n)
{
	return (x >> n) | (x << (32 - n));
}

CSHA256::CSHA256 (void)
:	m_nBlockLength (0),
	m_nTotalBytes (0)
{
	static const uint32_t Initial[8] =
	{
		0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
		0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19
	};

	for (unsigned i = 0; i < 8; i++)
	{
		m_State[i] = Initial[i];
	}
}

void CSHA256::Transform (const uint8_t *pBlock)
{
	uint32_t W[64];
	for (unsigned i = 0; i < 16; i++)
	{
		W[i] =   (uint32_t) pBlock[4*i] << 24 | (uint32_t) pBlock[4*i+1] << 16
		       | (uint32_t) pBlock[4*i+2] << 8 | (uint32_t) pBlock[4*i+3];
	}
	for (unsigned i = 16; i < 64; i++)
	{
		uint32_t s0 = Rotr (W[i-15], 7) ^ Rotr (W[i-15], 18) ^ (W[i-15] >> 3);
		uint32_t s1 = Rotr (W[i-2], 17) ^ Rotr (W[i-2], 19) ^ (W[i-2] >> 10);
		W[i] = W[i-16] + s0 + W[i-7] + s1;
	}

	uint32_t a = m_State[0], b = m_State[1], c = m_State[2], d = m_State[3];
	uint32_t e = m_State[4], f = m_State[5], g = m_State[6], h = m_State[7];

	for (unsigned i = 0; i < 64; i++)
	{
		uint32_t S1 = Rotr (e, 6) ^ Rotr (e, 11) ^ Rotr (e, 25);
		uint32_t ch = (e & f) ^ (~e & g);
		uint32_t t1 = h + S1 + ch + K[i] + W[i];
		uint32_t S0 = Rotr (a, 2) ^ Rotr (a, 13) ^ Rotr (a, 22);
		uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
		uint32_t t2 = S0 + maj;

		h = g; g = f; f = e; e = d + t1;
		d = c; c = b; b = a; a = t1 + t2;
	}

	m_State[0] += a; m_State[1] += b; m_State[2] += c; m_State[3] += d;
	m_State[4] += e; m_State[5] += f; m_State[6] += g; m_State[7] += h;
}

void CSHA256::Update (const void *pData, size_t nLength)
{
	const uint8_t *p = static_cast<const uint8_t *> (pData);
	m_nTotalBytes += nLength;

	while (nLength > 0)
	{
		size_t nCopy = 64 - m_nBlockLength;
		if (nCopy > nLength)
		{
			nCopy = nLength;
		}
		for (size_t i = 0; i < nCopy; i++)
		{
			m_Block[m_nBlockLength + i] = p[i];
		}
		m_nBlockLength += nCopy;
		p += nCopy;
		nLength -= nCopy;

		if (m_nBlockLength == 64)
		{
			Transform (m_Block);
			m_nBlockLength = 0;
		}
	}
}

void CSHA256::Final (uint8_t Digest[SHA256_DIGEST_SIZE])
{
	uint64_t nBits = m_nTotalBytes * 8;

	m_Block[m_nBlockLength++] = 0x80;
	if (m_nBlockLength > 56)
	{
		while (m_nBlockLength < 64)
		{
			m_Block[m_nBlockLength++] = 0;
		}
		Transform (m_Block);
		m_nBlockLength = 0;
	}
	while (m_nBlockLength < 56)
	{
		m_Block[m_nBlockLength++] = 0;
	}
	for (int i = 7; i >= 0; i--)
	{
		m_Block[m_nBlockLength++] = (uint8_t) (nBits >> (8 * i));
	}
	Transform (m_Block);

	for (unsigned i = 0; i < 8; i++)
	{
		Digest[4*i]   = (uint8_t) (m_State[i] >> 24);
		Digest[4*i+1] = (uint8_t) (m_State[i] >> 16);
		Digest[4*i+2] = (uint8_t) (m_State[i] >> 8);
		Digest[4*i+3] = (uint8_t) m_State[i];
	}
}

void CSHA256::FinalHex (char Hex[SHA256_HEX_SIZE])
{
	static const char Digits[] = "0123456789abcdef";
	uint8_t Digest[SHA256_DIGEST_SIZE];
	Final (Digest);

	for (unsigned i = 0; i < SHA256_DIGEST_SIZE; i++)
	{
		Hex[2*i]   = Digits[Digest[i] >> 4];
		Hex[2*i+1] = Digits[Digest[i] & 0x0F];
	}
	Hex[2 * SHA256_DIGEST_SIZE] = '\0';
}
