/*
 *  test_irtxqueue.cpp
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
#include "irtxqueue.h"
#include <memory>

namespace
{
	std::unique_ptr<TIRFrame> MakeFrame (uint16_t nFirstTiming)
	{
		std::unique_ptr<TIRFrame> Frame (new TIRFrame ());
		Frame->nFreqHz = 38000;
		Frame->nCount = 2;
		Frame->Timings[0] = nFirstTiming;
		Frame->Timings[1] = 560;
		return Frame;
	}
}

TEST (TxQueueKeepsArrivalOrder)
{
	std::unique_ptr<CIRTxQueue> Queue (new CIRTxQueue ());
	std::unique_ptr<TIRFrame> Out (new TIRFrame ());

	CHECK (Queue->Push (*MakeFrame (1)));
	CHECK (Queue->Push (*MakeFrame (2)));
	CHECK (Queue->Push (*MakeFrame (3)));

	for (uint16_t nExpected = 1; nExpected <= 3; nExpected++)
	{
		CHECK (Queue->Pop (Out.get ()));
		CHECK_EQ (Out->Timings[0], nExpected);
		CHECK_EQ (Out->nCount, 2u);
	}
	CHECK (!Queue->Pop (Out.get ()));
}

TEST (TxQueueDropsWhenFull)
{
	std::unique_ptr<CIRTxQueue> Queue (new CIRTxQueue ());
	std::unique_ptr<TIRFrame> Out (new TIRFrame ());

	for (uint16_t i = 1; i <= IR_TX_QUEUE_DEPTH; i++)
	{
		CHECK (Queue->Push (*MakeFrame (i)));
	}
	CHECK (!Queue->Push (*MakeFrame (99)));		// fifth: dropped
	CHECK_EQ (Queue->GetCount (), (unsigned) IR_TX_QUEUE_DEPTH);

	for (uint16_t i = 1; i <= IR_TX_QUEUE_DEPTH; i++)
	{
		CHECK (Queue->Pop (Out.get ()));
		CHECK_EQ (Out->Timings[0], i);
	}
}

TEST (TxQueueWrapsAround)
{
	std::unique_ptr<CIRTxQueue> Queue (new CIRTxQueue ());
	std::unique_ptr<TIRFrame> Out (new TIRFrame ());

	for (uint16_t i = 1; i <= 10; i++)
	{
		CHECK (Queue->Push (*MakeFrame (i)));
		CHECK (Queue->Pop (Out.get ()));
		CHECK_EQ (Out->Timings[0], i);
	}
}
