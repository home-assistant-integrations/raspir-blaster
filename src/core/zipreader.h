/*
 *  zipreader.h
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

#ifndef _raspir_zipreader_h
#define _raspir_zipreader_h

#include <stddef.h>
#include <stdint.h>

enum TZipResult
{
	ZipOK,
	ZipNotFound,		// file not in the archive
	ZipInvalidFormat,	// not a valid ZIP file or truncated
	ZipOutputFull,		// output buffer too small
	ZipCRCError,		// extracted data fails CRC-32 check
	ZipUnsupported,		// unsupported compression (not stored or deflate)
	ZipInflateError,	// DEFLATE decompression failed
};

/// Finds and extracts a file from a ZIP archive held in memory.
/// Verifies the CRC-32 of the extracted data. pOutputLen receives
/// the number of bytes written on ZipOK.
TZipResult ZipExtract (const uint8_t *pZip, size_t nZipLen,
                       const char *pName,
                       uint8_t *pOutput, size_t nOutputMax,
                       size_t *pOutputLen);

#endif
