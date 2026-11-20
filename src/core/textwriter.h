/*
 *  textwriter.h
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

#ifndef _raspir_textwriter_h
#define _raspir_textwriter_h

#include <stddef.h>
#include <stdint.h>

/// Small string helpers, so src/core needs no C library.
size_t CoreStrLen (const char *pString);
bool CoreStrEqual (const char *pString1, const char *pString2);
/// Compares a non-terminated string of given length with a C string.
bool CoreStrEqualN (const char *pString, size_t nLength, const char *pCString);

/// Appends text to a fixed-size buffer and keeps it NUL-terminated.
/// Once the buffer is full, further output is dropped and Overflow() is true.
class CTextWriter
{
public:
	/// \param pBuffer Destination buffer
	/// \param nSize   Buffer size in bytes, including the terminating NUL (>= 1)
	CTextWriter (char *pBuffer, size_t nSize);

	void Append (const char *pString);
	void Append (const char *pString, size_t nLength);
	void AppendChar (char chChar);
	void AppendUnsigned (uint32_t nValue);
	/// Appends a JSON string literal (with quotes), escaping as needed.
	void AppendJSONString (const char *pString);

	bool Overflow (void) const		{ return m_bOverflow; }
	size_t GetLength (void) const		{ return m_nLength; }
	const char *Get (void) const		{ return m_pBuffer; }

private:
	char   *m_pBuffer;
	size_t  m_nSize;
	size_t  m_nLength;
	bool    m_bOverflow;
};

#endif
