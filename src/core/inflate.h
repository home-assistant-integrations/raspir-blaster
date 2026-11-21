/*
 *  inflate.h
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

#ifndef _raspir_inflate_h
#define _raspir_inflate_h

#include <stddef.h>
#include <stdint.h>

enum TInflateResult
{
	InflateOK,
	InflateInvalidInput,	// malformed stream (bad symbol, bad checksum, reserved block type)
	InflateOutputFull,	// output buffer too small
	InflateTruncated,	// input ended prematurely
};

/// Decompresses a raw DEFLATE stream (RFC 1951, no zlib/gzip wrapper).
/// Supports stored (type 0), fixed Huffman (type 1) and dynamic Huffman (type 2) blocks.
/// pOutputLen receives the number of bytes written on InflateOK.
TInflateResult Inflate (const uint8_t *pInput, size_t nInputLen,
                        uint8_t *pOutput, size_t nOutputMax,
                        size_t *pOutputLen);

#endif
