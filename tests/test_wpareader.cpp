/*
 *  test_wpareader.cpp
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
#include "wpareader.h"
#include <cstring>

TEST (WPAConfigReadRoundtripsPSKNetwork)
{
	TWiFiConfig W;
	std::strncpy (W.SSID, "Home", sizeof W.SSID - 1);
	std::strncpy (W.PSK, "secret123", sizeof W.PSK - 1);
	W.Country[0] = 'D'; W.Country[1] = 'E'; W.Country[2] = '\0';

	char Buf[512];
	ConfigWriteWPA (W, Buf, sizeof Buf);

	TWiFiStatus Status;
	WPAConfigRead (Buf, std::strlen (Buf), &Status);

	CHECK (Status.bConfigured);
	CHECK (Status.bPasswordSet);
	CHECK_STR (Status.SSID, "Home");
	CHECK_STR (Status.Country, "DE");
}

TEST (WPAConfigReadRoundtripsOpenNetwork)
{
	TWiFiConfig W;
	std::strncpy (W.SSID, "OpenNet", sizeof W.SSID - 1);
	W.PSK[0] = '\0';
	W.Country[0] = 'U'; W.Country[1] = 'S'; W.Country[2] = '\0';

	char Buf[512];
	ConfigWriteWPA (W, Buf, sizeof Buf);

	TWiFiStatus Status;
	WPAConfigRead (Buf, std::strlen (Buf), &Status);

	CHECK (Status.bConfigured);
	CHECK (!Status.bPasswordSet);
	CHECK_STR (Status.SSID, "OpenNet");
	CHECK_STR (Status.Country, "US");
}

TEST (WPAConfigReadEmptyTextIsUnconfigured)
{
	TWiFiStatus Status;
	WPAConfigRead ("", 0, &Status);

	CHECK (!Status.bConfigured);
	CHECK_STR (Status.SSID, "");
	CHECK_STR (Status.Country, "");
}

TEST (WPAConfigReadIgnoresUnrelatedLines)
{
	const char *pText =
		"ctrl_interface=DIR=/var/run/wpa_supplicant GROUP=netdev\n"
		"update_config=1\n"
		"country=GB\n"
		"\n"
		"network={\n"
		"\tssid=\"Office\"\n"
		"\tkey_mgmt=NONE\n"
		"}\n";

	TWiFiStatus Status;
	WPAConfigRead (pText, std::strlen (pText), &Status);

	CHECK (Status.bConfigured);
	CHECK_STR (Status.SSID, "Office");
	CHECK_STR (Status.Country, "GB");
}

TEST (WPAConfigReadNeverExposesPassword)
{
	TWiFiConfig W;
	std::strncpy (W.SSID, "Home", sizeof W.SSID - 1);
	std::strncpy (W.PSK, "verysecretpassword", sizeof W.PSK - 1);
	W.Country[0] = 'D'; W.Country[1] = 'E'; W.Country[2] = '\0';

	char Buf[512];
	ConfigWriteWPA (W, Buf, sizeof Buf);

	TWiFiStatus Status;
	WPAConfigRead (Buf, std::strlen (Buf), &Status);

	// TWiFiStatus has no password field at all; verify nothing leaked into
	// the fields it does have.
	CHECK (std::strstr (Status.SSID, "secret") == 0);
}
