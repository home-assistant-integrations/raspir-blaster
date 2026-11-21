/*
 *  config.h
 *  raspir-blaster core
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

#ifndef _raspir_config_h
#define _raspir_config_h

#include <stddef.h>
#include <stdint.h>

#define CONFIG_FILE_NAME	"raspir-blaster.conf"

#define CONFIG_HOST_MAX		63
#define CONFIG_USER_MAX		63
#define CONFIG_PASSWORD_MAX	63
#define CONFIG_DEVICE_ID_MAX	31
#define CONFIG_TOPIC_BASE_MAX	63

#define CONFIG_DEFAULT_PORT		1883
#define CONFIG_DEFAULT_DEVICE_ID	"pi-zero-ir"
#define CONFIG_DEFAULT_TOPIC_BASE	"home/ir_blaster"
#define CONFIG_DEFAULT_GAP_MS		15
#define CONFIG_MIN_GAP_MS		5
#define CONFIG_MAX_GAP_MS		60

#define CONFIG_OTA_PASSWORD_MAX		63
#define CONFIG_OTA_PASSWORD_MIN		8

/// Effective device configuration (see specs/device-config).
struct TConfig
{
	char     MQTTHost[CONFIG_HOST_MAX + 1];
	unsigned nMQTTPort;
	bool     bHasUser;
	char     MQTTUser[CONFIG_USER_MAX + 1];
	bool     bHasPassword;
	char     MQTTPassword[CONFIG_PASSWORD_MAX + 1];
	char     DeviceID[CONFIG_DEVICE_ID_MAX + 1];
	char     TopicBase[CONFIG_TOPIC_BASE_MAX + 1];
	unsigned nIRFrameGapMs;
	bool     bHasOTAPassword;
	char     OTAPassword[CONFIG_OTA_PASSWORD_MAX + 1];
};

/// Receives warnings and errors found while parsing (never contains passwords).
typedef void TConfigMessageHandler (void *pParam, const char *pMessage);

/// Sets every key to its default (mqtt_host empty).
void ConfigSetDefaults (TConfig *pConfig);

/// Parses the key=value text of the configuration file into pConfig.
/// Unknown keys and invalid values are reported through pHandler (may be 0)
/// and fall back to their defaults.
/// \return true if the configuration is usable (a valid mqtt_host was found)
bool ConfigParse (const char *pText, size_t nLength, TConfig *pConfig,
		  TConfigMessageHandler *pHandler, void *pParam);

/// Writes the effective configuration as one line for logging, with the
/// password masked as "***" (omitted if not set).
void ConfigDump (const TConfig &Config, char *pBuffer, size_t nSize);

/// Validates and applies one key/value pair with the same rules as
/// ConfigParse(), but strictly: on an invalid or unrecognized key it
/// leaves *pConfig untouched and returns false, instead of falling back
/// to a default. For POST /api/config, where the whole request must be
/// refused (not partially applied) if any field is invalid.
bool ConfigValidateField (const char *pKey, const char *pValue, TConfig *pConfig);

#endif
