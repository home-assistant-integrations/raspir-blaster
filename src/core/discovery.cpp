/*
 *  discovery.cpp
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

#include "discovery.h"
#include "textwriter.h"

namespace
{
	size_t Finish (CTextWriter &Writer, char *pBuffer, size_t nSize)
	{
		if (Writer.Overflow ())
		{
			if (nSize > 0)
			{
				pBuffer[0] = '\0';
			}
			return 0;
		}

		return Writer.GetLength ();
	}

	/// Appends "<key>":"<topic_base>/<suffix>" with JSON escaping.
	void AppendTopicField (CTextWriter &Writer, const char *pKey,
			       const char *pTopicBase, const char *pSuffix)
	{
		char Topic[TOPIC_MAX];
		MQTTTopicBuild (pTopicBase, pSuffix, Topic, sizeof Topic);

		Writer.AppendJSONString (pKey);
		Writer.AppendChar (':');
		Writer.AppendJSONString (Topic);
	}

	/// Appends the fields shared by every discovery config, from
	/// ',"availability_topic"' to the closing brace (see specs/ha-discovery,
	/// "Device metadata": the device object is identical in all configs).
	void AppendCommonTail (CTextWriter &Writer, const char *pDeviceID, const char *pTopicBase,
			       const char *pModel, const char *pVersion)
	{
		Writer.AppendChar (',');
		AppendTopicField (Writer, "availability_topic", pTopicBase, TOPIC_SUFFIX_STATUS);
		Writer.Append (",\"icon\":\"mdi:remote\","
			       "\"device\":{\"identifiers\":[\"");
		Writer.Append (pDeviceID);
		Writer.Append ("\"],\"name\":\"raspir-blaster (");
		Writer.Append (pDeviceID);
		Writer.Append (")\",\"model\":");
		Writer.AppendJSONString (pModel);
		Writer.Append (",\"manufacturer\":\"DIY\",\"sw_version\":");
		Writer.AppendJSONString (pVersion);
		Writer.Append ("}}");
	}

	size_t BuildDiscoveryTopic (const char *pComponent, const char *pDeviceID,
				    char *pBuffer, size_t nSize)
	{
		CTextWriter Writer (pBuffer, nSize);
		Writer.Append ("homeassistant/");
		Writer.Append (pComponent);
		Writer.AppendChar ('/');
		Writer.Append (pDeviceID);
		Writer.Append ("/received/config");

		return Finish (Writer, pBuffer, nSize);
	}
}

size_t MQTTTopicBuild (const char *pTopicBase, const char *pSuffix, char *pBuffer, size_t nSize)
{
	CTextWriter Writer (pBuffer, nSize);
	Writer.Append (pTopicBase);
	Writer.AppendChar ('/');
	Writer.Append (pSuffix);

	return Finish (Writer, pBuffer, nSize);
}

size_t DiscoveryBuildTopic (const char *pDeviceID, char *pBuffer, size_t nSize)
{
	return BuildDiscoveryTopic ("sensor", pDeviceID, pBuffer, nSize);
}

size_t DiscoveryBuildEventTopic (const char *pDeviceID, char *pBuffer, size_t nSize)
{
	return BuildDiscoveryTopic ("event", pDeviceID, pBuffer, nSize);
}

size_t DiscoveryBuildPayload (const char *pDeviceID, const char *pTopicBase,
			      const char *pModel, const char *pVersion,
			      char *pBuffer, size_t nSize)
{
	CTextWriter Writer (pBuffer, nSize);

	// device_id is restricted to [a-z0-9_], so it needs no escaping;
	// topic_base may contain any printable character.
	Writer.Append ("{\"name\":\"IR Receiver\",\"unique_id\":\"");
	Writer.Append (pDeviceID);
	Writer.Append ("_received\",");
	AppendTopicField (Writer, "state_topic", pTopicBase, "received");
	Writer.AppendChar (',');
	AppendTopicField (Writer, "json_attributes_topic", pTopicBase, "received");
	Writer.Append (",\"value_template\":\"{{ value_json.raw | length }}\","
		       "\"unit_of_measurement\":\"pulses\"");
	AppendCommonTail (Writer, pDeviceID, pTopicBase, pModel, pVersion);

	return Finish (Writer, pBuffer, nSize);
}

size_t DiscoveryBuildEventPayload (const char *pDeviceID, const char *pTopicBase,
				   const char *pModel, const char *pVersion,
				   char *pBuffer, size_t nSize)
{
	CTextWriter Writer (pBuffer, nSize);

	// device_id is restricted to [a-z0-9_], so it needs no escaping.
	Writer.Append ("{\"name\":\"IR Event\",\"unique_id\":\"");
	Writer.Append (pDeviceID);
	Writer.Append ("_ir_event\",");
	AppendTopicField (Writer, "state_topic", pTopicBase, TOPIC_SUFFIX_RECEIVED);
	Writer.Append (",\"event_types\":[\"received\"],\"value_template\":");
	// Home Assistant's MQTT event needs {"event_type":...}; the template adds it
	// to the unchanged <topic_base>/received payload (design D1).
	Writer.AppendJSONString ("{\"event_type\":\"received\","
				 "\"freq\":{{ value_json.freq }},"
				 "\"raw\":{{ value_json.raw | tojson }}}");
	Writer.Append (",\"device_class\":\"button\"");
	AppendCommonTail (Writer, pDeviceID, pTopicBase, pModel, pVersion);

	return Finish (Writer, pBuffer, nSize);
}
