/*
 *  configwriter.cpp
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

#include "configwriter.h"
#include "textwriter.h"

// ----- helpers ------------------------------------------------------------------

bool ConfigValueSafe (const char *pValue)
{
	if (pValue == 0 || *pValue == '\0')
		return false;
	for (const char *p = pValue; *p != '\0'; p++)
	{
		unsigned char ch = (unsigned char) *p;
		if (ch < 0x20 || ch == 0x7F || ch == '"')
			return false;
	}
	return true;
}

// Append key=value\n, returning false if pValue is unsafe.
static bool AppendLine (CTextWriter &W, const char *pKey, const char *pValue)
{
	if (!ConfigValueSafe (pValue))
		return false;
	W.Append (pKey);
	W.AppendChar ('=');
	W.Append (pValue);
	W.AppendChar ('\n');
	return true;
}

static bool AppendUnsignedLine (CTextWriter &W, const char *pKey, unsigned nValue)
{
	W.Append (pKey);
	W.AppendChar ('=');
	W.AppendUnsigned (nValue);
	W.AppendChar ('\n');
	return true;
}

// ----- ConfigWriteConf ----------------------------------------------------------

TConfigWriteResult ConfigWriteConf (const TConfig &Config,
                                    char *pBuf, size_t nSize)
{
	CTextWriter W (pBuf, nSize);

	if (!AppendLine (W, "mqtt_host", Config.MQTTHost))
		return ConfigWriteInvalidField;
	AppendUnsignedLine (W, "mqtt_port", Config.nMQTTPort);

	if (Config.bHasUser)
	{
		if (!AppendLine (W, "mqtt_user", Config.MQTTUser))
			return ConfigWriteInvalidField;
	}
	if (Config.bHasPassword)
	{
		if (!AppendLine (W, "mqtt_password", Config.MQTTPassword))
			return ConfigWriteInvalidField;
	}

	if (!AppendLine (W, "device_id", Config.DeviceID))
		return ConfigWriteInvalidField;
	if (!AppendLine (W, "topic_base", Config.TopicBase))
		return ConfigWriteInvalidField;
	AppendUnsignedLine (W, "ir_frame_gap_ms", Config.nIRFrameGapMs);

	if (Config.bHasOTAPassword)
	{
		if (!AppendLine (W, "ota_password", Config.OTAPassword))
			return ConfigWriteInvalidField;
	}

	if (W.Overflow ())
		return ConfigWriteBufferFull;

	return ConfigWriteOK;
}

// ----- ConfigWriteWPA -----------------------------------------------------------

// Write a WPA supplicant quoted string, refusing unsafe values.
static bool AppendWPAString (CTextWriter &W, const char *pKey, const char *pValue)
{
	if (!ConfigValueSafe (pValue))
		return false;
	W.AppendChar ('\t');
	W.Append (pKey);
	W.Append ("=\"");
	W.Append (pValue);
	W.Append ("\"\n");
	return true;
}

TConfigWriteResult ConfigWriteWPA (const TWiFiConfig &WiFi,
                                   char *pBuf, size_t nSize)
{
	// Validate country: 2 uppercase ASCII letters
	if (WiFi.Country[0] < 'A' || WiFi.Country[0] > 'Z' ||
	    WiFi.Country[1] < 'A' || WiFi.Country[1] > 'Z' ||
	    WiFi.Country[2] != '\0')
		return ConfigWriteInvalidField;

	if (!ConfigValueSafe (WiFi.SSID))
		return ConfigWriteInvalidField;

	bool bOpen = (WiFi.PSK[0] == '\0');
	if (!bOpen && !ConfigValueSafe (WiFi.PSK))
		return ConfigWriteInvalidField;

	CTextWriter W (pBuf, nSize);

	W.Append ("ctrl_interface=DIR=/var/run/wpa_supplicant GROUP=netdev\n");
	W.Append ("update_config=1\n");
	W.Append ("country=");
	W.AppendChar (WiFi.Country[0]);
	W.AppendChar (WiFi.Country[1]);
	W.AppendChar ('\n');
	W.AppendChar ('\n');
	W.Append ("network={\n");

	if (!AppendWPAString (W, "ssid", WiFi.SSID))
		return ConfigWriteInvalidField;

	if (bOpen)
	{
		W.Append ("\tkey_mgmt=NONE\n");
	}
	else
	{
		if (!AppendWPAString (W, "psk", WiFi.PSK))
			return ConfigWriteInvalidField;
	}

	W.Append ("}\n");

	if (W.Overflow ())
		return ConfigWriteBufferFull;

	return ConfigWriteOK;
}
