/*
 *  test_misc.cpp
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

// Tests for the carrier calculator, the reconnect backoff and the
// Home Assistant discovery builder.

#include "test.h"
#include "backoff.h"
#include "discovery.h"
#include "ircarrier.h"
#include "irmessage.h"
#include "jsontokenizer.h"

// --- 7.2 carrier -----------------------------------------------------------------

TEST (CarrierCommonFrequenciesBCM283x)
{
	TIRCarrier Carrier;

	CHECK (IRCarrierCompute (36000, IR_OSCILLATOR_BCM283X_HZ, &Carrier));
	CHECK_EQ (Carrier.nRange, 267u);
	CHECK (IRCarrierCompute (38000, IR_OSCILLATOR_BCM283X_HZ, &Carrier));
	CHECK_EQ (Carrier.nRange, 253u);
	CHECK_EQ (Carrier.nMarkValue, 84u);
	CHECK (IRCarrierCompute (40000, IR_OSCILLATOR_BCM283X_HZ, &Carrier));
	CHECK_EQ (Carrier.nRange, 240u);
	CHECK_EQ (Carrier.nActualFreqHz, 40000u);
}

TEST (CarrierCommonFrequenciesBCM2711)
{
	TIRCarrier Carrier;

	// 54 MHz / 2 = 27 MHz PWM clock
	CHECK (IRCarrierCompute (36000, IR_OSCILLATOR_BCM2711_HZ, &Carrier));
	CHECK_EQ (Carrier.nRange, 750u);
	CHECK_EQ (Carrier.nActualFreqHz, 36000u);
	CHECK (IRCarrierCompute (38000, IR_OSCILLATOR_BCM2711_HZ, &Carrier));
	CHECK_EQ (Carrier.nRange, 711u);
	CHECK_EQ (Carrier.nMarkValue, 237u);
	CHECK (IRCarrierCompute (40000, IR_OSCILLATOR_BCM2711_HZ, &Carrier));
	CHECK_EQ (Carrier.nRange, 675u);
}

TEST (CarrierAccuracyAndDutyOverWholeRange)
{
	const uint32_t Oscillators[] = { IR_OSCILLATOR_BCM283X_HZ, IR_OSCILLATOR_BCM2711_HZ };

	for (uint32_t nOscillator : Oscillators)
	{
		for (uint32_t nFreq = IR_MIN_FREQ_HZ; nFreq <= IR_MAX_FREQ_HZ; nFreq += 50)
		{
			TIRCarrier Carrier;
			CHECK (IRCarrierCompute (nFreq, nOscillator, &Carrier));

			double ErrorPercent = 100.0 * ((double) Carrier.nActualFreqHz - nFreq) / nFreq;
			CHECK (ErrorPercent > -1.0 && ErrorPercent < 1.0);

			double DutyPercent = 100.0 * Carrier.nMarkValue / Carrier.nRange;
			CHECK (DutyPercent >= 30.0 && DutyPercent <= 36.0);
		}
	}
}

TEST (CarrierRejectsOutOfRange)
{
	TIRCarrier Carrier;
	CHECK (!IRCarrierCompute (19999, IR_OSCILLATOR_BCM283X_HZ, &Carrier));
	CHECK (!IRCarrierCompute (60001, IR_OSCILLATOR_BCM2711_HZ, &Carrier));
	CHECK (!IRCarrierCompute (0, IR_OSCILLATOR_BCM283X_HZ, &Carrier));
}

// --- 7.3 backoff -----------------------------------------------------------------

TEST (BackoffSequence)
{
	CBackoff Backoff;
	const unsigned Expected[] = { 5000, 10000, 20000, 40000, 60000, 60000, 60000 };

	for (unsigned nExpected : Expected)
	{
		CHECK_EQ (Backoff.NextDelayMs (), nExpected);
	}
}

TEST (BackoffResetsAfterSuccess)
{
	CBackoff Backoff;
	Backoff.NextDelayMs ();
	Backoff.NextDelayMs ();
	Backoff.Reset ();

	CHECK_EQ (Backoff.NextDelayMs (), 5000u);
	CHECK_EQ (Backoff.NextDelayMs (), 10000u);
}

// --- 7.4 discovery ---------------------------------------------------------------

TEST (TopicsAreBuiltFromTopicBase)
{
	char Buffer[TOPIC_MAX];

	CHECK (MQTTTopicBuild ("home/ir_blaster", TOPIC_SUFFIX_SEND, Buffer, sizeof Buffer) > 0);
	CHECK_STR (Buffer, "home/ir_blaster/send");

	CHECK (DiscoveryBuildTopic ("living_ir", Buffer, sizeof Buffer) > 0);
	CHECK_STR (Buffer, "homeassistant/sensor/living_ir/received/config");
}

TEST (TopicBuildReportsOverflow)
{
	char Buffer[8];
	CHECK_EQ (MQTTTopicBuild ("home/ir_blaster", "send", Buffer, sizeof Buffer), 0u);
}

TEST (DiscoveryPayloadContainsSpecifiedFields)
{
	char Buffer[DISCOVERY_PAYLOAD_MAX];
	size_t nLength = DiscoveryBuildPayload ("pi_zero_ir", "home/ir_blaster",
						"Raspberry Pi Zero W (Circle bare-metal)", "0.1.0",
						Buffer, sizeof Buffer);

	CHECK (nLength > 0);
	CHECK_STR (Buffer,
		"{\"name\":\"IR Receiver\","
		"\"unique_id\":\"pi_zero_ir_received\","
		"\"state_topic\":\"home/ir_blaster/received\","
		"\"json_attributes_topic\":\"home/ir_blaster/received\","
		"\"value_template\":\"{{ value_json.raw | length }}\","
		"\"unit_of_measurement\":\"pulses\","
		"\"availability_topic\":\"home/ir_blaster/status\","
		"\"icon\":\"mdi:remote\","
		"\"device\":{\"identifiers\":[\"pi_zero_ir\"],"
		"\"name\":\"raspir-blaster (pi_zero_ir)\","
		"\"model\":\"Raspberry Pi Zero W (Circle bare-metal)\","
		"\"manufacturer\":\"DIY\",\"sw_version\":\"0.1.0\"}}");
}

TEST (DiscoveryPayloadIsValidJSONWithEscapedTopicBase)
{
	char Buffer[DISCOVERY_PAYLOAD_MAX];
	size_t nLength = DiscoveryBuildPayload ("bedroom_ir", "a\"b\\c", "Compute Module 4 (Circle bare-metal)",
						"1.2.3", Buffer, sizeof Buffer);
	CHECK (nLength > 0);
	CHECK (std::strstr (Buffer, "\"state_topic\":\"a\\\"b\\\\c/received\"") != nullptr);

	// Walk all tokens: the payload must be well-formed JSON.
	CJSONTokenizer Tokenizer (Buffer, nLength);
	TJSONToken Token;
	TJSONTokenType Type;
	int nDepth = 0;
	do
	{
		Type = Tokenizer.Next (&Token);
		CHECK (Type != JSONTokenError);
		if (Type == JSONTokenObjectBegin || Type == JSONTokenArrayBegin) nDepth++;
		if (Type == JSONTokenObjectEnd || Type == JSONTokenArrayEnd) nDepth--;
	}
	while (Type != JSONTokenEnd && Type != JSONTokenError);
	CHECK_EQ (nDepth, 0);
}

TEST (DiscoveryModelReflectsBoard)
{
	char Buffer[DISCOVERY_PAYLOAD_MAX];
	CHECK (DiscoveryBuildPayload ("pi_zero_ir", "home/ir_blaster",
				      "Raspberry Pi 3 Model B+ (Circle bare-metal)", "0.1.0",
				      Buffer, sizeof Buffer) > 0);
	CHECK (std::strstr (Buffer, "\"model\":\"Raspberry Pi 3 Model B+ (Circle bare-metal)\"")
	       != nullptr);
}

TEST (DiscoveryDiffersPerDevice)
{
	char Living[DISCOVERY_PAYLOAD_MAX];
	char Bedroom[DISCOVERY_PAYLOAD_MAX];
	DiscoveryBuildPayload ("living_ir", "home/living", "M", "0.1.0", Living, sizeof Living);
	DiscoveryBuildPayload ("bedroom_ir", "home/bedroom", "M", "0.1.0", Bedroom, sizeof Bedroom);

	CHECK (std::strstr (Living, "\"unique_id\":\"living_ir_received\"") != nullptr);
	CHECK (std::strstr (Bedroom, "\"unique_id\":\"bedroom_ir_received\"") != nullptr);
	CHECK (std::strstr (Bedroom, "\"identifiers\":[\"bedroom_ir\"]") != nullptr);
}

// --- add-ir-event-entity: event discovery ------------------------------------------

TEST (DiscoveryEventTopicIsBuiltFromDeviceID)
{
	char Buffer[TOPIC_MAX];
	CHECK (DiscoveryBuildEventTopic ("living_ir", Buffer, sizeof Buffer) > 0);
	CHECK_STR (Buffer, "homeassistant/event/living_ir/received/config");
}

TEST (DiscoveryEventPayloadContainsSpecifiedFields)
{
	char Buffer[DISCOVERY_PAYLOAD_MAX];
	size_t nLength = DiscoveryBuildEventPayload ("pi_zero_ir", "home/ir_blaster",
						     "Raspberry Pi Zero W (Circle bare-metal)", "0.1.0",
						     Buffer, sizeof Buffer);

	CHECK (nLength > 0);
	CHECK_STR (Buffer,
		"{\"name\":\"IR Event\","
		"\"unique_id\":\"pi_zero_ir_ir_event\","
		"\"state_topic\":\"home/ir_blaster/received\","
		"\"event_types\":[\"received\"],"
		"\"value_template\":\"{\\\"event_type\\\":\\\"received\\\","
		"\\\"freq\\\":{{ value_json.freq }},"
		"\\\"raw\\\":{{ value_json.raw | tojson }}}\","
		"\"device_class\":\"button\","
		"\"availability_topic\":\"home/ir_blaster/status\","
		"\"icon\":\"mdi:remote\","
		"\"device\":{\"identifiers\":[\"pi_zero_ir\"],"
		"\"name\":\"raspir-blaster (pi_zero_ir)\","
		"\"model\":\"Raspberry Pi Zero W (Circle bare-metal)\","
		"\"manufacturer\":\"DIY\",\"sw_version\":\"0.1.0\"}}");
}

TEST (DiscoveryWorstCaseFitsBuffer)
{
	char Buffer[DISCOVERY_PAYLOAD_MAX];
	// Longest allowed device_id (31) and topic_base (63).
	CHECK (DiscoveryBuildPayload ("abcdefghijklmnopqrstuvwxyz01234",
				      "ttttttttttttttttttttttttttttttttttttttttttttttttttttttttttttttt",
				      "Raspberry Pi 3 Model B+ (Circle bare-metal)",
				      "999.999.999", Buffer, sizeof Buffer) > 0);
}

namespace
{
	/// Walks all tokens; true if the text is one well-formed JSON value.
	bool IsWellFormedJSON (const char *pText, size_t nLength)
	{
		CJSONTokenizer Tokenizer (pText, nLength);
		TJSONToken Token;
		TJSONTokenType Type;
		int nDepth = 0;
		do
		{
			Type = Tokenizer.Next (&Token);
			if (Type == JSONTokenObjectBegin || Type == JSONTokenArrayBegin) nDepth++;
			if (Type == JSONTokenObjectEnd || Type == JSONTokenArrayEnd) nDepth--;
		}
		while (Type != JSONTokenEnd && Type != JSONTokenError);

		return Type == JSONTokenEnd && nDepth == 0;
	}
}

TEST (DiscoveryEventPayloadIsValidJSONWithEscapedTopicBase)
{
	char Buffer[DISCOVERY_PAYLOAD_MAX];
	size_t nLength = DiscoveryBuildEventPayload ("bedroom_ir", "a\"b\\c",
						     "Compute Module 4 (Circle bare-metal)", "1.2.3",
						     Buffer, sizeof Buffer);
	CHECK (nLength > 0);
	CHECK (std::strstr (Buffer, "\"state_topic\":\"a\\\"b\\\\c/received\"") != nullptr);
	CHECK (std::strstr (Buffer, "\"availability_topic\":\"a\\\"b\\\\c/status\"") != nullptr);
	// The template's own quotes are escaped inside the JSON string.
	CHECK (std::strstr (Buffer, "\"value_template\":\"{\\\"event_type\\\":") != nullptr);
	CHECK (IsWellFormedJSON (Buffer, nLength));
}

TEST (DiscoveryEventPayloadFitsWorstCase)
{
	// Longest device_id, a topic_base of 63 characters that all need escaping,
	// and a model and version longer than any real one.
	char DeviceID[] = "abcdefghijklmnopqrstuvwxyz_0123";	// 31 characters
	char TopicBase[64];
	for (unsigned i = 0; i < 63; i++)
	{
		TopicBase[i] = i % 2 == 0 ? '"' : '\\';
	}
	TopicBase[63] = '\0';
	const char *pModel = "Raspberry Pi Compute Module 4 Rev 1.1 (Circle bare-metal)";
	const char *pVersion = "2026.12.10-dirty";

	char Buffer[DISCOVERY_PAYLOAD_MAX];
	size_t nLength = DiscoveryBuildEventPayload (DeviceID, TopicBase, pModel, pVersion,
						     Buffer, sizeof Buffer);
	CHECK (nLength > 0);
	CHECK (nLength < DISCOVERY_PAYLOAD_MAX);
	CHECK (IsWellFormedJSON (Buffer, nLength));

	// The sensor payload must fit the same worst case.
	nLength = DiscoveryBuildPayload (DeviceID, TopicBase, pModel, pVersion, Buffer, sizeof Buffer);
	CHECK (nLength > 0);
	CHECK (IsWellFormedJSON (Buffer, nLength));
}

TEST (DiscoveryEventPayloadReportsOverflow)
{
	char Buffer[64];
	CHECK_EQ (DiscoveryBuildEventPayload ("pi_zero_ir", "home/ir_blaster", "M", "0.1.0",
					      Buffer, sizeof Buffer), 0u);
	CHECK_STR (Buffer, "");
}

TEST (DiscoveryDeviceObjectIsIdenticalInBothPayloads)
{
	char Sensor[DISCOVERY_PAYLOAD_MAX];
	char Event[DISCOVERY_PAYLOAD_MAX];
	CHECK (DiscoveryBuildPayload ("living_ir", "home/living", "Raspberry Pi 4 Model B (Circle bare-metal)",
				      "2026.11.3", Sensor, sizeof Sensor) > 0);
	CHECK (DiscoveryBuildEventPayload ("living_ir", "home/living", "Raspberry Pi 4 Model B (Circle bare-metal)",
					   "2026.11.3", Event, sizeof Event) > 0);

	// Everything from "availability_topic" on (availability, icon, device) is shared.
	const char *pSensorTail = std::strstr (Sensor, "\"availability_topic\"");
	const char *pEventTail = std::strstr (Event, "\"availability_topic\"");
	CHECK (pSensorTail != nullptr);
	CHECK (pEventTail != nullptr);
	CHECK (std::strstr (pSensorTail, "\"device\":{") != nullptr);
	CHECK_STR (pEventTail, pSensorTail);
}

TEST (DiscoveryEventDiffersPerDevice)
{
	char Living[DISCOVERY_PAYLOAD_MAX];
	char Bedroom[DISCOVERY_PAYLOAD_MAX];
	DiscoveryBuildEventPayload ("living_ir", "home/living", "M", "0.1.0", Living, sizeof Living);
	DiscoveryBuildEventPayload ("bedroom_ir", "home/bedroom", "M", "0.1.0", Bedroom, sizeof Bedroom);

	CHECK (std::strstr (Living, "\"unique_id\":\"living_ir_ir_event\"") != nullptr);
	CHECK (std::strstr (Living, "\"state_topic\":\"home/living/received\"") != nullptr);
	CHECK (std::strstr (Bedroom, "\"unique_id\":\"bedroom_ir_ir_event\"") != nullptr);
	CHECK (std::strstr (Bedroom, "\"state_topic\":\"home/bedroom/received\"") != nullptr);
}
