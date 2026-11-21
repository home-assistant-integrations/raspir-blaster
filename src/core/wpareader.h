/*
 *  wpareader.h
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

#ifndef _raspir_wpareader_h
#define _raspir_wpareader_h

#include "configwriter.h"
#include <stddef.h>

/// Reads back the fields ConfigWriteWPA() writes: top-level "country=XX"
/// and, inside the "network={...}" block, 'ssid="..."'. Used for
/// GET /api/config to report the current Wi-Fi setup (never the password,
/// which wpa_supplicant.conf does not expose here either way). Missing
/// fields are left at their default ("" / zero-length).
struct TWiFiStatus
{
	bool bConfigured;	// a network block with a ssid was found
	bool bPasswordSet;	// the network block has a "psk=" line (not key_mgmt=NONE)
	char SSID[CONFIGWRITER_WIFI_SSID_MAX + 1];
	char Country[CONFIGWRITER_WIFI_COUNTRY_MAX + 1];
};

void WPAConfigRead (const char *pText, size_t nLength, TWiFiStatus *pStatus);

#endif
