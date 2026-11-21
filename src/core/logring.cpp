/*
 *  logring.cpp
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

#include "logring.h"

static_assert ((LOG_RING_SIZE & (LOG_RING_SIZE - 1)) == 0, "LOG_RING_SIZE must be a power of two");

// Index into the buffer; a mask instead of a 64-bit modulo (no libgcc call).
static inline size_t RingIndex (uint64_t nPosition)
{
	return (size_t) (nPosition & (LOG_RING_SIZE - 1));
}

CLogRing::CLogRing (void)
:	m_nWritten (0),
	m_nRead (0),
	m_nLost (0),
	m_bReading (false),
	m_chLast (0)
{
}

void CLogRing::Append (const char *pData, size_t nLength)
{
	for (size_t i = 0; i < nLength; i++)
	{
		char chByte = pData[i];
		if (chByte == '\n' && m_chLast != '\r')
		{
			Put ('\r');
		}

		Put (chByte);
	}
}

void CLogRing::Put (char chByte)
{
	m_Buffer[RingIndex (m_nWritten)] = chByte;
	m_nWritten++;
	m_chLast = chByte;

	// The writer has overwritten the oldest unread byte.
	if (m_bReading && m_nWritten - m_nRead > LOG_RING_SIZE)
	{
		m_nLost++;
		m_nRead++;
	}
}

void CLogRing::StartReader (void)
{
	m_nRead = m_nWritten > LOG_RING_SIZE ? m_nWritten - LOG_RING_SIZE : 0;
	m_nLost = 0;
	m_bReading = true;
}

void CLogRing::StopReader (void)
{
	m_bReading = false;
}

size_t CLogRing::Peek (const char **ppData) const
{
	if (!m_bReading || m_nRead == m_nWritten)
	{
		return 0;
	}

	size_t nStart = RingIndex (m_nRead);
	uint64_t nAvail = m_nWritten - m_nRead;
	size_t nToEnd = LOG_RING_SIZE - nStart;

	*ppData = &m_Buffer[nStart];

	return nAvail < nToEnd ? (size_t) nAvail : nToEnd;
}

void CLogRing::Consume (size_t nLength)
{
	uint64_t nAvail = m_nWritten - m_nRead;
	m_nRead += nLength < nAvail ? nLength : nAvail;
}

uint64_t CLogRing::TakeLost (void)
{
	uint64_t nLost = m_nLost;
	m_nLost = 0;

	return nLost;
}
