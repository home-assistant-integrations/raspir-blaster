/*
 *  configwriter.h
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

#ifndef _raspir_configwriter_h
#define _raspir_configwriter_h

#include "config.h"
#include <stddef.h>

#define CONFIGWRITER_WIFI_SSID_MAX	32
#define CONFIGWRITER_WIFI_PSK_MAX	63
#define CONFIGWRITER_WIFI_COUNTRY_MAX	2

/// Wi-Fi credentials for setup mode.
struct TWiFiConfig
{
	char SSID[CONFIGWRITER_WIFI_SSID_MAX + 1];
	char PSK[CONFIGWRITER_WIFI_PSK_MAX + 1];	// empty = open network
	char Country[CONFIGWRITER_WIFI_COUNTRY_MAX + 1];
};

enum TConfigWriteResult
{
	ConfigWriteOK,
	ConfigWriteInvalidField,	// field value contains '"' or a control character
	ConfigWriteBufferFull,
};

/// Writes raspir-blaster.conf content into pBuf (NUL-terminated, key=value lines).
/// Values are validated: '"' and control characters (< 0x20, 0x7F) are rejected.
/// pConfig must be valid (passes ConfigParse).
TConfigWriteResult ConfigWriteConf (const TConfig &Config,
                                    char *pBuf, size_t nSize);

/// Writes wpa_supplicant.conf content into pBuf (NUL-terminated).
/// psk empty → open network; otherwise WPA2-PSK.
/// Country must be 2 uppercase ASCII letters.
TConfigWriteResult ConfigWriteWPA (const TWiFiConfig &WiFi,
                                   char *pBuf, size_t nSize);

/// Validates a single field value for safe use in config files:
/// rejects '"' and control characters (< 0x20, 0x7F) and empty strings.
bool ConfigValueSafe (const char *pValue);

#endif
