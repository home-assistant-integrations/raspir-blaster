/*
 *  jsonwriter.h
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

#ifndef _raspir_jsonwriter_h
#define _raspir_jsonwriter_h

#include "textwriter.h"
#include <stddef.h>
#include <stdint.h>

#define JSON_MAX_DEPTH	4

/// Builds compact JSON (objects and arrays) into a fixed buffer, for the
/// API responses. Commas are inserted automatically. If the buffer is too
/// small, Overflow() is true and the output must not be used.
class CJSONWriter
{
public:
	CJSONWriter (char *pBuffer, size_t nSize);

	void BeginObject (const char *pKey = 0);	// key only inside objects
	void EndObject (void);
	void BeginArray (const char *pKey = 0);
	void EndArray (void);

	void String (const char *pKey, const char *pValue);	// pKey = 0 in arrays
	void Number (const char *pKey, uint32_t nValue);
	void Bool (const char *pKey, bool bValue);

	bool Overflow (void) const	{ return m_Writer.Overflow () || m_bError; }
	const char *Get (void) const	{ return m_Writer.Get (); }
	size_t GetLength (void) const	{ return m_Writer.GetLength (); }

private:
	void BeginValue (const char *pKey);
	void Push (char chOpen);
	void Pop (char chClose);

private:
	CTextWriter	m_Writer;
	unsigned	m_nDepth;
	bool		m_bFirst[JSON_MAX_DEPTH + 1];
	bool		m_bError;
};

#endif
