/*
 *  irmqttclient.h
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

#ifndef _irmqttclient_h
#define _irmqttclient_h

#include "backoff.h"
#include "config.h"
#include "discovery.h"
#include "irmessage.h"
#include "irtxqueue.h"
#include <circle/net/mqttclient.h>
#include <circle/net/netsubsystem.h>
#include <circle/types.h>

/// MQTT client of the IR blaster (see specs/connectivity, specs/ha-discovery).
///
/// The connection state machine runs in this client's own task (OnLoop()),
/// because CMQTTClient::Connect() resolves DNS and opens the TCP connection
/// synchronously. The main task, which feeds the watchdog, is never blocked.
class CIRMqttClient : public CMQTTClient
{
public:
	/// \param pModel Device model for discovery; must stay valid for the client's lifetime
	CIRMqttClient (CNetSubSystem *pNetSubSystem, const TConfig *pConfig, CIRTxQueue *pTxQueue,
		       const char *pModel);
	~CIRMqttClient (void);

	/// \return TRUE while connected to the broker
	boolean IsOnline (void) const		{ return m_State == StateConnected; }

	/// Publishes a received IR frame to <topic_base>/received (if online).
	void PublishFrame (const TIRFrame &Frame);

	void OnConnect (boolean bSessionPresent) override;
	void OnDisconnect (TMQTTDisconnectReason Reason) override;
	void OnMessage (const char *pTopic, const u8 *pPayload, size_t nPayloadLength,
			boolean bRetain) override;
	void OnLoop (void) override;

private:
	enum TState
	{
		StateNoNetwork,
		StateIdle,		// waiting for the next connection attempt
		StateConnecting,	// waiting for CONNACK
		StateConnected
	};

	static u64 NowMs (void);
	void StartConnect (void);
	void PublishText (const char *pTopic, const char *pText, boolean bRetain);

private:
	CNetSubSystem    *m_pNetSubSystem;
	const TConfig    *m_pConfig;
	CIRTxQueue       *m_pTxQueue;
	const char       *m_pModel;

	volatile TState   m_State;
	CBackoff          m_Backoff;
	u64               m_nNextAttemptMs;
	u64               m_nConnectStartMs;

	char              m_TopicSend[TOPIC_MAX];
	char              m_TopicReceived[TOPIC_MAX];
	char              m_TopicStatus[TOPIC_MAX];
	char              m_TopicDiscovery[TOPIC_MAX];

	TIRFrame          m_MessageFrame;			// parse buffer for OnMessage()
	char              m_PublishBuffer[IR_MAX_PAYLOAD];
};

#endif
