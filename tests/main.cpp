/*
 *  main.cpp
 *  raspir-blaster tests
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

#include "test.h"

CTestRegistry &CTestRegistry::Get (void)
{
	static CTestRegistry s_Registry;
	return s_Registry;
}

void CTestRegistry::Add (TTestCase *pTest)
{
	if (m_pLast == nullptr)
	{
		m_pFirst = pTest;
	}
	else
	{
		m_pLast->pNext = pTest;
	}

	m_pLast = pTest;
}

void CTestRegistry::Fail (const char *pFile, int nLine, const char *pExpression)
{
	std::printf ("    FAILED %s:%d: %s\n", pFile, nLine, pExpression);
	m_bCurrentFailed = true;
	m_nFailedChecks++;
}

int CTestRegistry::RunAll (void)
{
	unsigned nTests = 0;
	unsigned nFailedTests = 0;

	for (TTestCase *pTest = m_pFirst; pTest != nullptr; pTest = pTest->pNext)
	{
		m_bCurrentFailed = false;
		pTest->pFunction ();

		nTests++;
		if (m_bCurrentFailed)
		{
			nFailedTests++;
			std::printf ("[FAIL] %s\n", pTest->pName);
		}
	}

	std::printf ("%u tests, %u failed (%u failed checks)\n",
		     nTests, nFailedTests, m_nFailedChecks);

	return nFailedTests == 0 ? 0 : 1;
}

int main (void)
{
	return CTestRegistry::Get ().RunAll ();
}
