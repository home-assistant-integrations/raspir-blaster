/*
 *  test.h
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

// Minimal host-side unit test framework.
//
//   TEST (MyTest) { CHECK (1 + 1 == 2); CHECK_EQ (Value, 42u); CHECK_STR (p, "x"); }
//
// All tests are registered automatically; tests/main.cpp runs them and
// exits non-zero if any check failed.

#ifndef _raspir_test_h
#define _raspir_test_h

#include <cstdio>
#include <cstring>
#include <string>

typedef void TTestFunction (void);

struct TTestCase
{
	const char    *pName;
	TTestFunction *pFunction;
	TTestCase     *pNext;
};

class CTestRegistry
{
public:
	static CTestRegistry &Get (void);

	void Add (TTestCase *pTest);
	int RunAll (void);
	void Fail (const char *pFile, int nLine, const char *pExpression);

private:
	TTestCase *m_pFirst = nullptr;
	TTestCase *m_pLast = nullptr;
	bool       m_bCurrentFailed = false;
	unsigned   m_nFailedChecks = 0;
};

struct CTestRegistrar
{
	CTestRegistrar (TTestCase *pTest)	{ CTestRegistry::Get ().Add (pTest); }
};

#define TEST(Name)								\
	static void Name (void);						\
	static TTestCase Name##_Case = { #Name, Name, nullptr };		\
	static CTestRegistrar Name##_Registrar (&Name##_Case);		\
	static void Name (void)

#define CHECK(Expression)							\
	do {									\
		if (!(Expression))						\
			CTestRegistry::Get ().Fail (__FILE__, __LINE__, #Expression); \
	} while (0)

#define CHECK_EQ(Actual, Expected)						\
	do {									\
		if (!((Actual) == (Expected)))					\
		{								\
			std::printf ("    actual:   %lld\n    expected: %lld\n",	\
				     (long long) (Actual), (long long) (Expected)); \
			CTestRegistry::Get ().Fail (__FILE__, __LINE__,	\
						    #Actual " == " #Expected);	\
		}								\
	} while (0)

// The values are copied, so temporaries (e.g. a member of a returned struct)
// stay valid for the comparison.
#define CHECK_STR(Actual, Expected)						\
	do {									\
		const std::string Actual_ (Actual);				\
		const std::string Expected_ (Expected);				\
		if (Actual_ != Expected_)					\
		{								\
			std::printf ("    actual:   \"%s\"\n    expected: \"%s\"\n", \
				     Actual_.c_str (), Expected_.c_str ());	\
			CTestRegistry::Get ().Fail (__FILE__, __LINE__,	\
						    #Actual " == " #Expected);	\
		}								\
	} while (0)

#endif
