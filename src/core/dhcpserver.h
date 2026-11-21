/*
 *  dhcpserver.h
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

#ifndef _raspir_dhcpserver_h
#define _raspir_dhcpserver_h

#include <stddef.h>
#include <stdint.h>

// Pool is 192.168.4.10 – 192.168.4.50 (41 addresses); server is 192.168.4.1.
#define DHCP_POOL_SIZE	41u
#define DHCP_LEASE_TIME	3600u

/// Minimal DHCP server for setup-mode (open AP at 192.168.4.1).
/// Hardware-independent: takes raw UDP payload bytes, returns reply bytes.
/// A reply to broadcast (giaddr == 0) is sent to 255.255.255.255:68.
class CDHCPServer
{
public:
	CDHCPServer (void);

	/// Process one received DHCP packet (raw UDP payload, port 67).
	/// pReply must be at least 300 bytes. Returns reply length, or 0
	/// if no reply should be sent (e.g. malformed request or message
	/// addressed to another server).
	size_t Process (const uint8_t *pRequest, size_t nRequestLen,
	                uint8_t *pReply, size_t nReplyMax);

	/// Number of active leases (for inactivity detection).
	unsigned GetLeaseCount (void) const;

private:
	struct TLease
	{
		uint8_t  MAC[6];
		uint8_t  nSuffix;	// last octet of 192.168.4.nSuffix
		bool     bActive;
	};

	TLease m_Leases[DHCP_POOL_SIZE];

	// Return lease suffix (10–50) for MAC, allocating a new one if needed.
	// Returns 0 on pool exhaustion.
	uint8_t AllocateSuffix (const uint8_t *pMAC);

	// Find an existing active lease for a MAC (returns slot index or -1).
	int FindMAC (const uint8_t *pMAC) const;

	static void WriteOption (uint8_t *&p, uint8_t nType,
	                         const uint8_t *pVal, uint8_t nLen);
	static void WriteIP     (uint8_t *&p, uint8_t nType,
	                         const uint8_t *pIP);
	static void WriteU32    (uint8_t *&p, uint8_t nType, uint32_t nVal);
};

#endif
