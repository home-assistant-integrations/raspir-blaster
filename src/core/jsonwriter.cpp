/*
 *  jsonwriter.cpp
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

#include "jsonwriter.h"

CJSONWriter::CJSONWriter (char *pBuffer, size_t nSize)
:	m_Writer (pBuffer, nSize),
	m_nDepth (0),
	m_bError (false)
{
	m_bFirst[0] = true;
}

void CJSONWriter::BeginValue (const char *pKey)
{
	if (!m_bFirst[m_nDepth])
	{
		m_Writer.AppendChar (',');
	}
	m_bFirst[m_nDepth] = false;

	if (pKey != 0)
	{
		m_Writer.AppendJSONString (pKey);
		m_Writer.AppendChar (':');
	}
}

void CJSONWriter::Push (char chOpen)
{
	m_Writer.AppendChar (chOpen);
	if (m_nDepth >= JSON_MAX_DEPTH)
	{
		m_bError = true;
		return;
	}
	m_bFirst[++m_nDepth] = true;
}

void CJSONWriter::Pop (char chClose)
{
	m_Writer.AppendChar (chClose);
	if (m_nDepth == 0)
	{
		m_bError = true;
		return;
	}
	m_nDepth--;
}

void CJSONWriter::BeginObject (const char *pKey)
{
	BeginValue (pKey);
	Push ('{');
}

void CJSONWriter::EndObject (void)
{
	Pop ('}');
}

void CJSONWriter::BeginArray (const char *pKey)
{
	BeginValue (pKey);
	Push ('[');
}

void CJSONWriter::EndArray (void)
{
	Pop (']');
}

void CJSONWriter::String (const char *pKey, const char *pValue)
{
	BeginValue (pKey);
	m_Writer.AppendJSONString (pValue);
}

void CJSONWriter::Number (const char *pKey, uint32_t nValue)
{
	BeginValue (pKey);
	m_Writer.AppendUnsigned (nValue);
}

void CJSONWriter::Bool (const char *pKey, bool bValue)
{
	BeginValue (pKey);
	m_Writer.Append (bValue ? "true" : "false");
}
