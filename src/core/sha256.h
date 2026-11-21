/*
 *  sha256.h
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

#ifndef _raspir_sha256_h
#define _raspir_sha256_h

#include <stddef.h>
#include <stdint.h>

#define SHA256_DIGEST_SIZE	32
#define SHA256_HEX_SIZE		(2 * SHA256_DIGEST_SIZE + 1)	// with NUL

/// SHA-256 (FIPS 180-4), incremental.
class CSHA256
{
public:
	CSHA256 (void);

	void Update (const void *pData, size_t nLength);

	/// Finishes the hash; the object must not be updated afterwards.
	void Final (uint8_t Digest[SHA256_DIGEST_SIZE]);

	/// Finishes the hash and writes it as 64 lower-case hex digits + NUL.
	void FinalHex (char Hex[SHA256_HEX_SIZE]);

private:
	void Transform (const uint8_t *pBlock);

private:
	uint32_t m_State[8];
	uint8_t	 m_Block[64];
	size_t	 m_nBlockLength;
	uint64_t m_nTotalBytes;
};

#endif
