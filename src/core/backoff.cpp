/*
 *  backoff.cpp
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

#include "backoff.h"

CBackoff::CBackoff (unsigned nInitialMs, unsigned nMaxMs)
:	m_nInitialMs (nInitialMs),
	m_nMaxMs (nMaxMs),
	m_nNextMs (nInitialMs)
{
}

unsigned CBackoff::NextDelayMs (void)
{
	unsigned nDelayMs = m_nNextMs;

	m_nNextMs = nDelayMs >= m_nMaxMs / 2 ? m_nMaxMs : nDelayMs * 2;

	return nDelayMs;
}

void CBackoff::Reset (void)
{
	m_nNextMs = m_nInitialMs;
}
