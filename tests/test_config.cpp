/*
 *  test_config.cpp
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
#include "config.h"
#include <string>
#include <vector>

namespace
{
	struct TParsed
	{
		bool                     bValid;
		TConfig                  Config;
		std::vector<std::string> Messages;
	};

	void CollectMessage (void *pParam, const char *pMessage)
	{
		static_cast<std::vector<std::string> *> (pParam)->push_back (pMessage);
	}

	TParsed Parse (const char *pText)
	{
		TParsed Result;
		Result.bValid = ConfigParse (pText, std::strlen (pText), &Result.Config,
					     CollectMessage, &Result.Messages);
		return Result;
	}

	bool HasMessage (const TParsed &Parsed, const char *pMessage)
	{
		for (const std::string &Message : Parsed.Messages)
		{
			if (Message == pMessage)
			{
				return true;
			}
		}

		std::printf ("    messages:\n");
		for (const std::string &Message : Parsed.Messages)
		{
			std::printf ("      \"%s\"\n", Message.c_str ());
		}

		return false;
	}
}

// --- 5.1 line parsing -------------------------------------------------------

TEST (ConfigParsesKeyValueWithWhitespaceAndCRLF)
{
	TParsed P = Parse ("mqtt_host = 192.168.1.10\r\n  mqtt_port=1883  \r\n");

	CHECK (P.bValid);
	CHECK_STR (P.Config.MQTTHost, "192.168.1.10");
	CHECK_EQ (P.Config.nMQTTPort, 1883u);
	CHECK (P.Messages.empty ());
}

TEST (ConfigIgnoresCommentsAndBlankLines)
{
	TParsed P = Parse ("# broker\n\n   \n  # indented comment\nmqtt_host=broker.local\n\n");

	CHECK (P.bValid);
	CHECK_STR (P.Config.MQTTHost, "broker.local");
	CHECK (P.Messages.empty ());
}

TEST (ConfigHandlesMissingFinalNewline)
{
	TParsed P = Parse ("mqtt_host=10.0.0.1\nmqtt_port=8883");

	CHECK (P.bValid);
	CHECK_EQ (P.Config.nMQTTPort, 8883u);
}

TEST (ConfigValueMayContainEqualsAndHash)
{
	TParsed P = Parse ("mqtt_host=h\nmqtt_user=u\nmqtt_password=a=b#c\n");

	CHECK (P.bValid);
	CHECK_STR (P.Config.MQTTPassword, "a=b#c");
}

TEST (ConfigWarnsAboutUnknownKey)
{
	TParsed P = Parse ("mqtt_host=h\ncolour=blue\n");

	CHECK (P.bValid);
	CHECK (HasMessage (P, "line 2: unknown key 'colour' ignored"));
}

TEST (ConfigWarnsAboutLineWithoutEquals)
{
	TParsed P = Parse ("mqtt_host=h\njust some text\n");

	CHECK (P.bValid);
	CHECK (HasMessage (P, "line 2: missing '=' - line ignored"));
}

TEST (ConfigLastDuplicateWins)
{
	TParsed P = Parse ("mqtt_host=first\nmqtt_host=second\n");

	CHECK_STR (P.Config.MQTTHost, "second");
}

// --- 5.2 defaults and validation --------------------------------------------

TEST (ConfigDefaultsWithOnlyRequiredKey)
{
	TParsed P = Parse ("mqtt_host=192.168.1.100\n");

	CHECK (P.bValid);
	CHECK_EQ (P.Config.nMQTTPort, 1883u);
	CHECK (!P.Config.bHasUser);
	CHECK (!P.Config.bHasPassword);
	CHECK_STR (P.Config.DeviceID, "pi_zero_ir");
	CHECK_STR (P.Config.TopicBase, "home/ir_blaster");
	CHECK_EQ (P.Config.nIRFrameGapMs, 15u);
}

TEST (ConfigAcceptsAllValidValues)
{
	TParsed P = Parse ("mqtt_host=mqtt-broker.home.arpa\n"
			   "mqtt_port=65535\n"
			   "mqtt_user=ha\n"
			   "mqtt_password=secret\n"
			   "device_id=living_ir_2\n"
			   "topic_base=house/living/ir\n"
			   "ir_frame_gap_ms=60\n");

	CHECK (P.bValid);
	CHECK (P.Messages.empty ());
	CHECK_EQ (P.Config.nMQTTPort, 65535u);
	CHECK (P.Config.bHasUser);
	CHECK_STR (P.Config.MQTTUser, "ha");
	CHECK (P.Config.bHasPassword);
	CHECK_STR (P.Config.DeviceID, "living_ir_2");
	CHECK_STR (P.Config.TopicBase, "house/living/ir");
	CHECK_EQ (P.Config.nIRFrameGapMs, 60u);
}

TEST (ConfigOutOfRangeFallsBackToDefault)
{
	TParsed P = Parse ("mqtt_host=h\nir_frame_gap_ms=200\nmqtt_port=0\n");

	CHECK (P.bValid);
	CHECK_EQ (P.Config.nIRFrameGapMs, 15u);
	CHECK_EQ (P.Config.nMQTTPort, 1883u);
	CHECK (HasMessage (P, "line 2: invalid value '200' for 'ir_frame_gap_ms' - using default"));
	CHECK (HasMessage (P, "line 3: invalid value '0' for 'mqtt_port' - using default"));
}

TEST (ConfigRejectsMalformedNumbers)
{
	TParsed P = Parse ("mqtt_host=h\nmqtt_port=18x3\nir_frame_gap_ms=\n");

	CHECK_EQ (P.Config.nMQTTPort, 1883u);
	CHECK_EQ (P.Config.nIRFrameGapMs, 15u);
	CHECK_EQ (P.Messages.size (), 2u);
}

TEST (ConfigGapRangeBoundaries)
{
	CHECK_EQ (Parse ("mqtt_host=h\nir_frame_gap_ms=5\n").Config.nIRFrameGapMs, 5u);
	CHECK_EQ (Parse ("mqtt_host=h\nir_frame_gap_ms=4\n").Config.nIRFrameGapMs, 15u);
	CHECK_EQ (Parse ("mqtt_host=h\nir_frame_gap_ms=61\n").Config.nIRFrameGapMs, 15u);
}

TEST (ConfigValidatesDeviceID)
{
	CHECK_STR (Parse ("mqtt_host=h\ndevice_id=Living\n").Config.DeviceID, "pi_zero_ir");
	CHECK_STR (Parse ("mqtt_host=h\ndevice_id=a-b\n").Config.DeviceID, "pi_zero_ir");
	CHECK_STR (Parse ("mqtt_host=h\ndevice_id=abcdefghijklmnopqrstuvwxyz01234\n").Config.DeviceID,
		   "abcdefghijklmnopqrstuvwxyz01234");
	CHECK_STR (Parse ("mqtt_host=h\ndevice_id=abcdefghijklmnopqrstuvwxyz012345\n").Config.DeviceID,
		   "pi_zero_ir");
}

TEST (ConfigValidatesTopicBase)
{
	CHECK_STR (Parse ("mqtt_host=h\ntopic_base=a/+/b\n").Config.TopicBase, "home/ir_blaster");
	CHECK_STR (Parse ("mqtt_host=h\ntopic_base=a/#\n").Config.TopicBase, "home/ir_blaster");
	CHECK_STR (Parse ("mqtt_host=h\ntopic_base=a/b/\n").Config.TopicBase, "home/ir_blaster");
	CHECK_STR (Parse ("mqtt_host=h\ntopic_base=a b\n").Config.TopicBase, "home/ir_blaster");
	CHECK_STR (Parse ("mqtt_host=h\ntopic_base=x\n").Config.TopicBase, "x");
}

TEST (ConfigValidatesHostAndCredentialLengths)
{
	const char *p64 = "mqtt_host=aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\n";
	CHECK (!Parse (p64).bValid);

	TParsed P = Parse ("mqtt_host=h\n"
			   "mqtt_user=uuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuu\n");
	CHECK (P.bValid);
	CHECK (!P.Config.bHasUser);
}

TEST (ConfigPasswordWithoutUserIsIgnored)
{
	TParsed P = Parse ("mqtt_host=h\nmqtt_password=secret\n");

	CHECK (P.bValid);
	CHECK (!P.Config.bHasUser);
	CHECK (HasMessage (P, "'mqtt_password' is ignored because 'mqtt_user' is not set"));
}

TEST (ConfigInvalidPasswordIsNotEchoed)
{
	TParsed P = Parse ("mqtt_host=h\nmqtt_user=u\n"
			   "mqtt_password=pppppppppppppppppppppppppppppppppppppppppppppppppppppppppppppppp\n");

	CHECK (!P.Config.bHasPassword);
	CHECK (HasMessage (P, "line 3: invalid value for 'mqtt_password' - ignored"));
	for (const std::string &Message : P.Messages)
	{
		CHECK (Message.find ("pppp") == std::string::npos);
	}
}

// --- 5.3 required key and masked dump ---------------------------------------

TEST (ConfigMissingHostIsInvalid)
{
	TParsed P = Parse ("mqtt_port=1883\n");

	CHECK (!P.bValid);
	CHECK (HasMessage (P, "missing required key 'mqtt_host'"));
}

TEST (ConfigInvalidHostIsInvalid)
{
	TParsed P = Parse ("mqtt_host=bad host!\n");

	CHECK (!P.bValid);
	CHECK (HasMessage (P, "missing required key 'mqtt_host'"));
}

TEST (ConfigEmptyFileIsInvalid)
{
	CHECK (!Parse ("").bValid);
}

TEST (ConfigDumpMasksPassword)
{
	TParsed P = Parse ("mqtt_host=h\nmqtt_user=u\nmqtt_password=topsecret\n");
	char Buffer[512];
	ConfigDump (P.Config, Buffer, sizeof Buffer);

	CHECK_STR (Buffer, "mqtt_host=h mqtt_port=1883 mqtt_user=u mqtt_password=*** "
			   "device_id=pi_zero_ir topic_base=home/ir_blaster ir_frame_gap_ms=15");
}

TEST (ConfigDumpOmitsUnsetCredentials)
{
	TParsed P = Parse ("mqtt_host=h\n");
	char Buffer[512];
	ConfigDump (P.Config, Buffer, sizeof Buffer);

	CHECK_STR (Buffer, "mqtt_host=h mqtt_port=1883 "
			   "device_id=pi_zero_ir topic_base=home/ir_blaster ir_frame_gap_ms=15");
}
