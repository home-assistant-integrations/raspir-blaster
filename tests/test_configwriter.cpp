/*
 *  test_configwriter.cpp
 *  raspir-blaster tests
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

#include "test.h"
#include "configwriter.h"
#include "config.h"
#include <cstring>
#include <string>
#include <vector>

namespace
{
	void CollectMsg (void *pParam, const char *pMsg)
	{
		static_cast<std::vector<std::string> *>(pParam)->push_back (pMsg);
	}

	struct TParsed
	{
		bool                     bOK;
		TConfig                  Config;
		std::vector<std::string> Messages;
	};

	TParsed Parse (const char *pText)
	{
		TParsed P;
		P.bOK = ConfigParse (pText, std::strlen (pText), &P.Config,
		                     CollectMsg, &P.Messages);
		return P;
	}

	TConfig MakeValidConfig (void)
	{
		TConfig c;
		ConfigSetDefaults (&c);
		std::strncpy (c.MQTTHost, "mqtt.home.arpa", sizeof c.MQTTHost - 1);
		c.nMQTTPort = 1883;
		c.bHasUser = true;
		std::strncpy (c.MQTTUser, "ha_user", sizeof c.MQTTUser - 1);
		c.bHasPassword = true;
		std::strncpy (c.MQTTPassword, "s3cr3t", sizeof c.MQTTPassword - 1);
		std::strncpy (c.DeviceID, "living-ir", sizeof c.DeviceID - 1);
		std::strncpy (c.TopicBase, "home/ir", sizeof c.TopicBase - 1);
		c.nIRFrameGapMs = 20;
		c.bHasOTAPassword = true;
		std::strncpy (c.OTAPassword, "updatepass1", sizeof c.OTAPassword - 1);
		return c;
	}
}

// ----- ConfigValueSafe ----------------------------------------------------------

TEST (ConfigValueSafeAcceptsNormalStrings)
{
	CHECK (ConfigValueSafe ("hello"));
	CHECK (ConfigValueSafe ("mqtt-broker.home.arpa"));
	CHECK (ConfigValueSafe ("s3cr3t!"));
	// boundary: 0x20 (space) accepted, 0x7E (~) accepted
	char s[3] = {0x20, 0x7E, 0};
	CHECK (ConfigValueSafe (s));
}

TEST (ConfigValueSafeRejectsEmpty)
{
	CHECK (!ConfigValueSafe (""));
	CHECK (!ConfigValueSafe (0));
}

TEST (ConfigValueSafeRejectsControlChars)
{
	char s[3];
	for (unsigned ch = 1; ch < 0x20u; ch++)
	{
		s[0] = (char) ch; s[1] = 'x'; s[2] = 0;
		CHECK (!ConfigValueSafe (s));
	}
	// 0x7F DEL
	s[0] = (char) 0x7F; s[1] = 0;
	CHECK (!ConfigValueSafe (s));
}

TEST (ConfigValueSafeRejectsDoubleQuote)
{
	CHECK (!ConfigValueSafe ("bad\"value"));
	CHECK (!ConfigValueSafe ("\""));
}

// ----- ConfigWriteConf ----------------------------------------------------------

TEST (ConfigWriteConfProducesValidOutput)
{
	TConfig c = MakeValidConfig ();
	char buf[1024];
	TConfigWriteResult r = ConfigWriteConf (c, buf, sizeof buf);
	CHECK_EQ ((int) r, (int) ConfigWriteOK);

	std::string s (buf);
	CHECK (s.find ("mqtt_host=mqtt.home.arpa\n") != std::string::npos);
	CHECK (s.find ("mqtt_port=1883\n") != std::string::npos);
	CHECK (s.find ("mqtt_user=ha_user\n") != std::string::npos);
	CHECK (s.find ("mqtt_password=s3cr3t\n") != std::string::npos);
	CHECK (s.find ("device_id=living-ir\n") != std::string::npos);
	CHECK (s.find ("topic_base=home/ir\n") != std::string::npos);
	CHECK (s.find ("ir_frame_gap_ms=20\n") != std::string::npos);
	CHECK (s.find ("ota_password=updatepass1\n") != std::string::npos);
}

TEST (ConfigWriteConfOmitsUnsetCredentials)
{
	TConfig c = MakeValidConfig ();
	c.bHasUser        = false;
	c.bHasPassword    = false;
	c.bHasOTAPassword = false;
	char buf[1024];
	TConfigWriteResult r = ConfigWriteConf (c, buf, sizeof buf);
	CHECK_EQ ((int) r, (int) ConfigWriteOK);

	std::string s (buf);
	CHECK (s.find ("mqtt_user") == std::string::npos);
	CHECK (s.find ("mqtt_password") == std::string::npos);
	CHECK (s.find ("ota_password") == std::string::npos);
}

TEST (ConfigWriteConfRoundtrips)
{
	TConfig c = MakeValidConfig ();
	char buf[1024];
	ConfigWriteConf (c, buf, sizeof buf);

	TParsed P = Parse (buf);
	CHECK (P.bOK);
	CHECK_STR (P.Config.MQTTHost, c.MQTTHost);
	CHECK_EQ (P.Config.nMQTTPort, c.nMQTTPort);
	CHECK_STR (P.Config.MQTTUser, c.MQTTUser);
	CHECK_STR (P.Config.MQTTPassword, c.MQTTPassword);
	CHECK_STR (P.Config.DeviceID, c.DeviceID);
	CHECK_STR (P.Config.TopicBase, c.TopicBase);
	CHECK_EQ (P.Config.nIRFrameGapMs, c.nIRFrameGapMs);
	CHECK (P.Config.bHasOTAPassword);
	CHECK_STR (P.Config.OTAPassword, c.OTAPassword);
}

TEST (ConfigWriteConfBufferFull)
{
	TConfig c = MakeValidConfig ();
	char buf[16];
	TConfigWriteResult r = ConfigWriteConf (c, buf, sizeof buf);
	CHECK_EQ ((int) r, (int) ConfigWriteBufferFull);
}

// ----- ConfigWriteWPA -----------------------------------------------------------

TEST (ConfigWriteWPAProducesValidOutput)
{
	TWiFiConfig w;
	std::strncpy (w.SSID, "MyNetwork", sizeof w.SSID - 1);
	std::strncpy (w.PSK, "MyPassphrase", sizeof w.PSK - 1);
	w.Country[0] = 'D'; w.Country[1] = 'E'; w.Country[2] = '\0';

	char buf[512];
	TConfigWriteResult r = ConfigWriteWPA (w, buf, sizeof buf);
	CHECK_EQ ((int) r, (int) ConfigWriteOK);

	std::string s (buf);
	CHECK (s.find ("ctrl_interface=") != std::string::npos);
	CHECK (s.find ("country=DE\n") != std::string::npos);
	CHECK (s.find ("ssid=\"MyNetwork\"\n") != std::string::npos);
	CHECK (s.find ("psk=\"MyPassphrase\"\n") != std::string::npos);
	CHECK (s.find ("key_mgmt") == std::string::npos);
}

TEST (ConfigWriteWPAOpenNetwork)
{
	TWiFiConfig w;
	std::strncpy (w.SSID, "OpenNet", sizeof w.SSID - 1);
	w.PSK[0] = '\0';
	w.Country[0] = 'U'; w.Country[1] = 'S'; w.Country[2] = '\0';

	char buf[512];
	TConfigWriteResult r = ConfigWriteWPA (w, buf, sizeof buf);
	CHECK_EQ ((int) r, (int) ConfigWriteOK);

	std::string s (buf);
	CHECK (s.find ("key_mgmt=NONE\n") != std::string::npos);
	CHECK (s.find ("psk=") == std::string::npos);
}

TEST (ConfigWriteWPARejectsInvalidCountry)
{
	TWiFiConfig w;
	std::strncpy (w.SSID, "Net", sizeof w.SSID - 1);
	w.PSK[0] = '\0';
	char buf[512];

	// lowercase
	std::strncpy (w.Country, "de", sizeof w.Country - 1);
	CHECK_EQ ((int) ConfigWriteWPA (w, buf, sizeof buf),
	          (int) ConfigWriteInvalidField);

	// only one letter + NUL
	w.Country[0] = 'D'; w.Country[1] = '\0'; w.Country[2] = '\0';
	CHECK_EQ ((int) ConfigWriteWPA (w, buf, sizeof buf),
	          (int) ConfigWriteInvalidField);
}

TEST (ConfigWriteWPARejectsUnsafeSSID)
{
	TWiFiConfig w;
	w.SSID[0] = '"'; w.SSID[1] = 0;
	w.PSK[0] = '\0';
	w.Country[0] = 'U'; w.Country[1] = 'S'; w.Country[2] = '\0';
	char buf[512];
	CHECK_EQ ((int) ConfigWriteWPA (w, buf, sizeof buf),
	          (int) ConfigWriteInvalidField);
}

TEST (ConfigWriteWPABufferFull)
{
	TWiFiConfig w;
	std::strncpy (w.SSID, "Net", sizeof w.SSID - 1);
	w.PSK[0] = '\0';
	w.Country[0] = 'G'; w.Country[1] = 'B'; w.Country[2] = '\0';
	char buf[8];
	CHECK_EQ ((int) ConfigWriteWPA (w, buf, sizeof buf),
	          (int) ConfigWriteBufferFull);
}

// ----- ota_password in ConfigParse (task 4.1) ------------------------------------

TEST (OTAPasswordAcceptsValidLength)
{
	TParsed P = Parse ("mqtt_host=h\nota_password=ValidPass1\n");
	CHECK (P.bOK);
	CHECK (P.Config.bHasOTAPassword);
	CHECK_STR (P.Config.OTAPassword, "ValidPass1");
	CHECK (P.Messages.empty ());
}

TEST (OTAPasswordTooShortIsIgnored)
{
	TParsed P = Parse ("mqtt_host=h\nota_password=short\n");	// 5 chars < 8
	CHECK (!P.Config.bHasOTAPassword);
	bool warnFound = false;
	for (auto &m : P.Messages)
	{
		if (m.find ("ota_password") != std::string::npos &&
		    m.find ("8") != std::string::npos)
			warnFound = true;
		CHECK (m.find ("short") == std::string::npos);	// never echo value
	}
	CHECK (warnFound);
}

TEST (OTAPasswordTooLongIsIgnored)
{
	// 64 chars — just over the 63-char maximum
	TParsed P = Parse ("mqtt_host=h\nota_password="
	                   "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA\n");
	CHECK (!P.Config.bHasOTAPassword);
}

TEST (OTAPasswordNotSetEmitsWarning)
{
	TParsed P = Parse ("mqtt_host=h\n");
	CHECK (!P.Config.bHasOTAPassword);
	bool warnFound = false;
	for (auto &m : P.Messages)
		if (m.find ("ota_password") != std::string::npos)
			warnFound = true;
	CHECK (warnFound);
}

TEST (OTAPasswordNeverEchoedInMessages)
{
	TParsed P = Parse ("mqtt_host=h\nota_password=s\n");	// too short
	for (auto &m : P.Messages)
		CHECK (m.find ("=s") == std::string::npos);
}

TEST (OTAPasswordDumpMasked)
{
	TConfig c;
	ConfigSetDefaults (&c);
	std::strncpy (c.MQTTHost, "h", sizeof c.MQTTHost - 1);
	c.bHasOTAPassword = true;
	std::strncpy (c.OTAPassword, "secretpass", sizeof c.OTAPassword - 1);

	char buf[256];
	ConfigDump (c, buf, sizeof buf);
	std::string s (buf);
	CHECK (s.find ("ota_password=***") != std::string::npos);
	CHECK (s.find ("secretpass") == std::string::npos);
}

TEST (OTAPasswordDumpNotSet)
{
	TConfig c;
	ConfigSetDefaults (&c);
	std::strncpy (c.MQTTHost, "h", sizeof c.MQTTHost - 1);

	char buf[256];
	ConfigDump (c, buf, sizeof buf);
	std::string s (buf);
	CHECK (s.find ("ota_password=(not set)") != std::string::npos);
}
