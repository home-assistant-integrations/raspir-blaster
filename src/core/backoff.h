/*
 *  backoff.h
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

#ifndef _raspir_backoff_h
#define _raspir_backoff_h

#define RECONNECT_INITIAL_DELAY_MS	5000
#define RECONNECT_MAX_DELAY_MS		60000

/// Exponential reconnect backoff: 5, 10, 20, 40, 60, 60, ... seconds.
class CBackoff
{
public:
	CBackoff (unsigned nInitialMs = RECONNECT_INITIAL_DELAY_MS,
		  unsigned nMaxMs = RECONNECT_MAX_DELAY_MS);

	/// \return delay before the next attempt; doubles on every call up to the maximum
	unsigned NextDelayMs (void);

	/// Call after a successful connection.
	void Reset (void);

private:
	unsigned m_nInitialMs;
	unsigned m_nMaxMs;
	unsigned m_nNextMs;
};

#endif
