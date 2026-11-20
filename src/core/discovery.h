/*
 *  discovery.h
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

#ifndef _raspir_discovery_h
#define _raspir_discovery_h

#include <stddef.h>

#define TOPIC_SUFFIX_SEND	"send"
#define TOPIC_SUFFIX_RECEIVED	"received"
#define TOPIC_SUFFIX_STATUS	"status"

#define STATUS_ONLINE		"online"
#define STATUS_OFFLINE		"offline"

#define TOPIC_MAX		128	// buffer size for any topic built here
#define DISCOVERY_PAYLOAD_MAX	1024	// buffer size for the discovery payload

/// Builds "<topic_base>/<suffix>".
/// \return length, or 0 if the buffer is too small
size_t MQTTTopicBuild (const char *pTopicBase, const char *pSuffix, char *pBuffer, size_t nSize);

/// Builds "homeassistant/sensor/<device_id>/received/config".
/// \return length, or 0 if the buffer is too small
size_t DiscoveryBuildTopic (const char *pDeviceID, char *pBuffer, size_t nSize);

/// Builds the Home Assistant MQTT Discovery config for the receiver sensor
/// (see specs/ha-discovery).
/// \param pModel Device model, e.g. "Raspberry Pi 4 Model B (Circle bare-metal)"
/// \return length, or 0 if the buffer is too small
size_t DiscoveryBuildPayload (const char *pDeviceID, const char *pTopicBase,
			      const char *pModel, const char *pVersion,
			      char *pBuffer, size_t nSize);

#endif
