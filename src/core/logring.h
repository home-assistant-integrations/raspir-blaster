/*
 *  logring.h
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

#ifndef _raspir_logring_h
#define _raspir_logring_h

#include <stddef.h>
#include <stdint.h>

#define LOG_RING_SIZE	16384		// must be a power of two

/// Keeps the most recent LOG_RING_SIZE bytes of log text for the USB serial
/// port (openspec/specs/connectivity, "Diagnostic log on serial"). Line feeds
/// are stored as CR LF. One reader replays the kept text and then follows the
/// live output; if it falls behind by more than the capacity, the skipped
/// bytes are counted as lost. Not thread-safe: the caller serializes access.
class CLogRing
{
public:
	CLogRing (void);

	/// Appends log text; "\n" becomes "\r\n" unless it already follows "\r".
	void Append (const char *pData, size_t nLength);

	/// Starts reading at the oldest kept byte and clears the lost counter.
	void StartReader (void);
	void StopReader (void);
	bool IsReading (void) const	{ return m_bReading; }

	/// \param ppData receives a pointer to the next unread bytes
	/// \return number of contiguous unread bytes (0 if none or not reading)
	size_t Peek (const char **ppData) const;

	/// Marks nLength bytes returned by Peek() as sent.
	void Consume (size_t nLength);

	/// \return bytes overwritten before the reader got them, since the last call
	uint64_t TakeLost (void);

	/// \return total bytes appended (after CR LF conversion)
	uint64_t GetWritten (void) const	{ return m_nWritten; }

private:
	void Put (char chByte);

private:
	char		m_Buffer[LOG_RING_SIZE];
	uint64_t	m_nWritten;	// bytes ever written
	uint64_t	m_nRead;	// reader position (absolute)
	uint64_t	m_nLost;
	bool		m_bReading;
	char		m_chLast;	// last appended byte
};

#endif
