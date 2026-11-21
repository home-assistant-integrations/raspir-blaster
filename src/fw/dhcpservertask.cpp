/*
 *  dhcpservertask.cpp
 *  raspir-blaster firmware
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

#include "dhcpservertask.h"
#include <circle/logger.h>
#include <circle/net/in.h>
#include <circle/net/ipaddress.h>

#define RECEIVE_TIMEOUT_US	500000u	// 500 ms: how promptly RequestStop() is noticed
#define REQUEST_BUF_SIZE	576u	// RFC 2131 recommended max DHCP message size
#define REPLY_BUF_SIZE		300u	// matches CDHCPServer::Process()'s requirement

static const char FromDHCPTask[] = "dhcpsrv";

CDHCPServerTask::CDHCPServerTask (CNetSubSystem *pNet)
:	m_Socket (pNet, IPPROTO_UDP),
	m_bStopRequested (FALSE)
{
	SetName (FromDHCPTask);
}

CDHCPServerTask::~CDHCPServerTask (void)
{
}

void CDHCPServerTask::Run (void)
{
	if (m_Socket.Bind (DHCP_SERVER_PORT) < 0)
	{
		CLogger::Get ()->Write (FromDHCPTask, LogError,
		                       "Cannot bind DHCP socket (port %u)", DHCP_SERVER_PORT);
		return;
	}

	m_Socket.SetOptionBroadcast (TRUE);
	m_Socket.SetOptionReceiveTimeout (RECEIVE_TIMEOUT_US);

	CLogger::Get ()->Write (FromDHCPTask, LogNotice,
	                       "DHCP server listening on port %u", DHCP_SERVER_PORT);

	CIPAddress BroadcastIP;
	BroadcastIP.SetBroadcast ();

	while (!m_bStopRequested)
	{
		u8 Request[REQUEST_BUF_SIZE];
		int nReceived = m_Socket.Receive (Request, sizeof Request, 0);
		if (nReceived <= 0)
		{
			continue;	// timeout or transient error: just re-check the stop flag
		}

		u8 Reply[REPLY_BUF_SIZE];
		size_t nReplyLen = m_Server.Process (Request, (size_t) nReceived, Reply, sizeof Reply);
		if (nReplyLen > 0)
		{
			m_Socket.SendTo (Reply, (unsigned) nReplyLen, 0, BroadcastIP, DHCP_CLIENT_PORT);
		}
	}

	CLogger::Get ()->Write (FromDHCPTask, LogNotice, "DHCP server stopped");
}
