/*
 *  test_textwriter.cpp
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
#include "textwriter.h"

TEST (TextWriterAppendsAndTerminates)
{
	char Buffer[32];
	CTextWriter Writer (Buffer, sizeof Buffer);
	Writer.Append ("freq=");
	Writer.AppendUnsigned (38000);
	Writer.AppendChar ('!');

	CHECK_STR (Writer.Get (), "freq=38000!");
	CHECK_EQ (Writer.GetLength (), 11u);
	CHECK (!Writer.Overflow ());
}

TEST (TextWriterUnsignedEdgeValues)
{
	char Buffer[32];
	CTextWriter Writer (Buffer, sizeof Buffer);
	Writer.AppendUnsigned (0);
	Writer.AppendChar (',');
	Writer.AppendUnsigned (4294967295u);

	CHECK_STR (Writer.Get (), "0,4294967295");
}

TEST (TextWriterOverflowKeepsTermination)
{
	char Buffer[6];
	CTextWriter Writer (Buffer, sizeof Buffer);
	Writer.Append ("123456789");

	CHECK (Writer.Overflow ());
	CHECK_STR (Writer.Get (), "12345");
}

TEST (TextWriterEscapesJSONStrings)
{
	char Buffer[64];
	CTextWriter Writer (Buffer, sizeof Buffer);
	Writer.AppendJSONString ("a\"b\\c\nd");

	CHECK_STR (Writer.Get (), "\"a\\\"b\\\\c\\u000ad\"");
}

TEST (StringHelpers)
{
	CHECK_EQ (CoreStrLen (""), 0u);
	CHECK_EQ (CoreStrLen ("raw"), 3u);
	CHECK (CoreStrEqual ("raw", "raw"));
	CHECK (!CoreStrEqual ("raw", "raws"));
	CHECK (CoreStrEqualN ("rawxyz", 3, "raw"));
	CHECK (!CoreStrEqualN ("raw", 3, "raws"));
	CHECK (!CoreStrEqualN ("ra", 2, "raw"));
}
