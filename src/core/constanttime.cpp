/*
 *  constanttime.cpp
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

#include "constanttime.h"

bool ConstantTimeEqual (const char *pA, const char *pB)
{
	size_t nA = 0;
	while (pA[nA] != '\0')
	{
		nA++;
	}

	size_t nB = 0;
	while (pB[nB] != '\0')
	{
		nB++;
	}

	size_t nMax = nA > nB ? nA : nB;

	unsigned char nDiff = (unsigned char) (nA != nB);
	for (size_t i = 0; i < nMax; i++)
	{
		unsigned char chA = (i < nA) ? (unsigned char) pA[i] : 0;
		unsigned char chB = (i < nB) ? (unsigned char) pB[i] : 0;
		nDiff = (unsigned char) (nDiff | (chA ^ chB));
	}

	return nDiff == 0;
}
