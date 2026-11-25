/*
 *  irmqttclient.cpp
 *  raspir-blaster firmware
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

#include "irmqttclient.h"
#include "version.h"
#include <circle/logger.h>
#include <circle/net/mqtt.h>
#include <circle/timer.h>
#include <circle/util.h>

// Room for a 4 KB payload plus topic and MQTT header.
#define MQTT_MAX_PACKET_SIZE	5120
#define MQTT_MAX_PACKETS_QUEUED	4
#define MQTT_MAX_TOPIC_SIZE	256
#define MQTT_KEEP_ALIVE_S	60
#define MQTT_CONNECT_TIMEOUT_MS	30000

// Circle's CMQTTClient::Publish() always appends a packet identifier, which
// is only valid for QoS >= 1, so every message is published with QoS 1.
#define MQTT_PUBLISH_QOS	MQTT_QOS1

static const char FromMQTT[] = "mqtt";

CIRMqttClient::CIRMqttClient (CNetSubSystem *pNetSubSystem, const TConfig *pConfig,
			      CIRTxQueue *pTxQueue, const char *pModel)
:	CMQTTClient (pNetSubSystem, MQTT_MAX_PACKET_SIZE, MQTT_MAX_PACKETS_QUEUED,
		     MQTT_MAX_TOPIC_SIZE),
	m_pNetSubSystem (pNetSubSystem),
	m_pConfig (pConfig),
	m_pTxQueue (pTxQueue),
	m_pModel (pModel),
	m_State (StateNoNetwork),
	m_nNextAttemptMs (0),
	m_nConnectStartMs (0)
{
	MQTTTopicBuild (pConfig->TopicBase, TOPIC_SUFFIX_SEND, m_TopicSend, sizeof m_TopicSend);
	MQTTTopicBuild (pConfig->TopicBase, TOPIC_SUFFIX_RECEIVED,
			m_TopicReceived, sizeof m_TopicReceived);
	MQTTTopicBuild (pConfig->TopicBase, TOPIC_SUFFIX_STATUS, m_TopicStatus, sizeof m_TopicStatus);
	DiscoveryBuildTopic (pConfig->DeviceID, m_TopicDiscovery, sizeof m_TopicDiscovery);
	DiscoveryBuildEventTopic (pConfig->DeviceID, m_TopicDiscoveryEvent,
				  sizeof m_TopicDiscoveryEvent);
}

CIRMqttClient::~CIRMqttClient (void)
{
}

u64 CIRMqttClient::NowMs (void)
{
	return CTimer::GetClockTicks64 () / 1000;
}

void CIRMqttClient::OnLoop (void)
{
	if (!m_pNetSubSystem->IsRunning ())
	{
		if (m_State != StateNoNetwork)
		{
			CLogger::Get ()->Write (FromMQTT, LogWarning, "Network down");

			if (m_State == StateConnecting || m_State == StateConnected)
			{
				Disconnect (TRUE);	// TCP is gone anyway: no DISCONNECT packet
			}
			m_State = StateNoNetwork;
		}

		return;
	}

	switch (m_State)
	{
	case StateNoNetwork:
		// Network (re)appeared: connect right away.
		m_State = StateIdle;
		m_nNextAttemptMs = NowMs ();
		break;

	case StateIdle:
		if (NowMs () >= m_nNextAttemptMs)
		{
			StartConnect ();
		}
		break;

	case StateConnecting:
		if (NowMs () - m_nConnectStartMs >= MQTT_CONNECT_TIMEOUT_MS)
		{
			CLogger::Get ()->Write (FromMQTT, LogWarning, "No CONNACK from broker");
			Disconnect (TRUE);	// calls OnDisconnect()
		}
		break;

	case StateConnected:
		break;
	}
}

void CIRMqttClient::StartConnect (void)
{
	CLogger::Get ()->Write (FromMQTT, LogNotice, "Connecting to %s:%u",
				m_pConfig->MQTTHost, m_pConfig->nMQTTPort);

	// Set the state first: Connect() calls OnDisconnect() directly on failure.
	m_State = StateConnecting;
	m_nConnectStartMs = NowMs ();

	const char *pUser = m_pConfig->bHasUser ? m_pConfig->MQTTUser : 0;
	const char *pPassword = m_pConfig->bHasUser && m_pConfig->bHasPassword
				? m_pConfig->MQTTPassword : 0;

	static const char WillPayload[] = STATUS_OFFLINE;

	Connect (m_pConfig->MQTTHost, (u16) m_pConfig->nMQTTPort, m_pConfig->DeviceID,
		 pUser, pPassword, MQTT_KEEP_ALIVE_S, TRUE,
		 m_TopicStatus, MQTT_QOS1, TRUE,
		 (const u8 *) WillPayload, sizeof WillPayload - 1);
}

void CIRMqttClient::OnConnect (boolean bSessionPresent)
{
	CLogger::Get ()->Write (FromMQTT, LogNotice, "Connected to broker");

	m_State = StateConnected;
	m_Backoff.Reset ();

	PublishText (m_TopicStatus, STATUS_ONLINE, TRUE);
	Subscribe (m_TopicSend, MQTT_QOS1);

	char Payload[DISCOVERY_PAYLOAD_MAX];
	if (DiscoveryBuildPayload (m_pConfig->DeviceID, m_pConfig->TopicBase, m_pModel,
				   RASPIR_FIRMWARE_VERSION, Payload, sizeof Payload) > 0)
	{
		PublishText (m_TopicDiscovery, Payload, TRUE);
	}
	else
	{
		CLogger::Get ()->Write (FromMQTT, LogError, "Discovery payload too large");
	}

	// Publish() copies the payload into its own packet, so the buffer can be reused.
	if (DiscoveryBuildEventPayload (m_pConfig->DeviceID, m_pConfig->TopicBase, m_pModel,
					RASPIR_FIRMWARE_VERSION, Payload, sizeof Payload) > 0)
	{
		PublishText (m_TopicDiscoveryEvent, Payload, TRUE);
	}
	else
	{
		CLogger::Get ()->Write (FromMQTT, LogError, "Event discovery payload too large");
	}
}

void CIRMqttClient::OnDisconnect (TMQTTDisconnectReason Reason)
{
	unsigned nDelayMs = m_Backoff.NextDelayMs ();

	CLogger::Get ()->Write (FromMQTT, LogWarning, "Disconnected (reason %u), retry in %u s",
				(unsigned) Reason, nDelayMs / 1000);

	m_State = StateIdle;
	m_nNextAttemptMs = NowMs () + nDelayMs;
}

void CIRMqttClient::OnMessage (const char *pTopic, const u8 *pPayload, size_t nPayloadLength,
			       boolean bRetain)
{
	if (strcmp (pTopic, m_TopicSend) != 0)
	{
		return;
	}

	if (bRetain)
	{
		// A stale retained command must never be replayed after a reconnect.
		CLogger::Get ()->Write (FromMQTT, LogWarning,
					"Ignoring retained message on %s", m_TopicSend);
		return;
	}

	TIRParseResult Result = IRMessageParse ((const char *) pPayload, nPayloadLength,
						&m_MessageFrame);
	if (Result != IRParseOK)
	{
		CLogger::Get ()->Write (FromMQTT, LogWarning, "Rejected message: %s",
					IRParseResultString (Result));
		return;
	}

	if (!m_pTxQueue->Push (m_MessageFrame))
	{
		CLogger::Get ()->Write (FromMQTT, LogWarning, "Transmit queue full, message dropped");
	}
}

void CIRMqttClient::PublishFrame (const TIRFrame &Frame)
{
	if (m_State != StateConnected)
	{
		return;
	}

	size_t nLength = IRMessageSerialize (Frame, m_PublishBuffer, sizeof m_PublishBuffer);
	if (nLength == 0)
	{
		CLogger::Get ()->Write (FromMQTT, LogError, "Frame does not fit into a message");
		return;
	}

	Publish (m_TopicReceived, (const u8 *) m_PublishBuffer, nLength, MQTT_PUBLISH_QOS, FALSE);
}

void CIRMqttClient::PublishText (const char *pTopic, const char *pText, boolean bRetain)
{
	Publish (pTopic, (const u8 *) pText, strlen (pText), MQTT_PUBLISH_QOS, bRetain);
}
