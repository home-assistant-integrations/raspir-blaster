/*
 *  dhcpservertask.h
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

#ifndef _raspir_dhcpservertask_h
#define _raspir_dhcpservertask_h

#include "dhcpserver.h"
#include <circle/net/netsubsystem.h>
#include <circle/net/socket.h>
#include <circle/sched/task.h>
#include <circle/types.h>

#define DHCP_SERVER_PORT	67
#define DHCP_CLIENT_PORT	68

/// Runs CDHCPServer (src/core) on a UDP socket for setup mode. Owned by
/// CKernel; created after CreateOpenNet() and destroyed (via Terminate(),
/// WaitForTermination()) before DestroyOpenNet().
class CDHCPServerTask : public CTask
{
public:
	CDHCPServerTask (CNetSubSystem *pNet);
	~CDHCPServerTask (void);

	void Run (void) override;

	/// Breaks the receive loop (checked every 500 ms); call then
	/// WaitForTermination() from the kernel's main task before deleting.
	void RequestStop (void)	{ m_bStopRequested = TRUE; }

private:
	CSocket           m_Socket;
	CDHCPServer       m_Server;
	volatile boolean  m_bStopRequested;
};

#endif
