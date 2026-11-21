/*
 *  dhcpserver.cpp
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

#include "dhcpserver.h"

// ----- DHCP message layout (RFC 2131) -------------------------------------------

#define DHCP_FIXED_SIZE		236u	// op through file, before magic cookie
#define DHCP_MAGIC_OFFSET	236u
#define DHCP_OPTIONS_OFFSET	240u
#define DHCP_MIN_SIZE		243u	// fixed + magic + end byte

// Fixed-header byte offsets
#define OFF_OP		0u
#define OFF_HTYPE	1u
#define OFF_HLEN	2u
#define OFF_HOPS	3u
#define OFF_XID		4u	// 4 bytes
#define OFF_SECS	8u	// 2 bytes
#define OFF_FLAGS	10u	// 2 bytes
#define OFF_CIADDR	12u	// 4 bytes
#define OFF_YIADDR	16u	// 4 bytes
#define OFF_SIADDR	20u	// 4 bytes
#define OFF_GIADDR	24u	// 4 bytes
#define OFF_CHADDR	28u	// 16 bytes (first 6 = MAC)

// op values
#define BOOTREQUEST	1u
#define BOOTREPLY	2u

// DHCP message type option values
#define DHCP_DISCOVER	1u
#define DHCP_OFFER	2u
#define DHCP_REQUEST	3u
#define DHCP_ACK	5u
#define DHCP_NAK	6u

// DHCP option codes
#define OPT_SUBNET	1u
#define OPT_ROUTER	3u
#define OPT_LEASE_TIME	51u
#define OPT_MSG_TYPE	53u
#define OPT_SERVER_ID	54u
#define OPT_REQ_IP	50u
#define OPT_END		255u
#define OPT_PAD		0u

// Magic cookie
static const uint8_t s_Magic[4] = {0x63, 0x82, 0x53, 0x63};

// Server IP (192.168.4.1) and subnet mask
static const uint8_t s_ServerIP[4] = {192, 168, 4, 1};
static const uint8_t s_Subnet[4]   = {255, 255, 255, 0};

// Pool: 192.168.4.{POOL_START .. POOL_START + DHCP_POOL_SIZE - 1}
#define POOL_START	10u

// ----- helpers ------------------------------------------------------------------

static bool MACEqual (const uint8_t *a, const uint8_t *b)
{
	for (unsigned i = 0; i < 6; i++)
		if (a[i] != b[i]) return false;
	return true;
}

static uint8_t ParseMsgType (const uint8_t *pOpts, size_t nOptsLen)
{
	for (size_t i = 0; i < nOptsLen; )
	{
		uint8_t nType = pOpts[i++];
		if (nType == OPT_END) break;
		if (nType == OPT_PAD) continue;
		if (i >= nOptsLen) break;
		uint8_t nLen = pOpts[i++];
		if (nType == OPT_MSG_TYPE && nLen >= 1)
			return pOpts[i];
		i += nLen;
	}
	return 0;
}

// Copy 4-byte IP from options (option type OPT_REQ_IP or OPT_SERVER_ID).
static bool ParseIP (const uint8_t *pOpts, size_t nOptsLen,
                     uint8_t nOptType, uint8_t *pIP)
{
	for (size_t i = 0; i < nOptsLen; )
	{
		uint8_t nType = pOpts[i++];
		if (nType == OPT_END) break;
		if (nType == OPT_PAD) continue;
		if (i >= nOptsLen) break;
		uint8_t nLen = pOpts[i++];
		if (nType == nOptType && nLen >= 4)
		{
			pIP[0] = pOpts[i + 0];
			pIP[1] = pOpts[i + 1];
			pIP[2] = pOpts[i + 2];
			pIP[3] = pOpts[i + 3];
			return true;
		}
		i += nLen;
	}
	return false;
}

// ----- CDHCPServer --------------------------------------------------------------

CDHCPServer::CDHCPServer (void)
{
	for (unsigned i = 0; i < DHCP_POOL_SIZE; i++)
	{
		for (unsigned j = 0; j < 6; j++)
			m_Leases[i].MAC[j] = 0;
		m_Leases[i].nSuffix = (uint8_t) (POOL_START + i);
		m_Leases[i].bActive = false;
	}
}

int CDHCPServer::FindMAC (const uint8_t *pMAC) const
{
	for (unsigned i = 0; i < DHCP_POOL_SIZE; i++)
	{
		if (m_Leases[i].bActive && MACEqual (m_Leases[i].MAC, pMAC))
			return (int) i;
	}
	return -1;
}

uint8_t CDHCPServer::AllocateSuffix (const uint8_t *pMAC)
{
	// Return existing lease for this MAC if present
	int nIdx = FindMAC (pMAC);
	if (nIdx >= 0)
		return m_Leases[nIdx].nSuffix;

	// Allocate the first free slot
	for (unsigned i = 0; i < DHCP_POOL_SIZE; i++)
	{
		if (!m_Leases[i].bActive)
		{
			for (unsigned j = 0; j < 6; j++)
				m_Leases[i].MAC[j] = pMAC[j];
			m_Leases[i].bActive = true;
			return m_Leases[i].nSuffix;
		}
	}
	return 0;	// pool exhausted
}

unsigned CDHCPServer::GetLeaseCount (void) const
{
	unsigned n = 0;
	for (unsigned i = 0; i < DHCP_POOL_SIZE; i++)
		if (m_Leases[i].bActive) n++;
	return n;
}

void CDHCPServer::WriteOption (uint8_t *&p, uint8_t nType,
                               const uint8_t *pVal, uint8_t nLen)
{
	*p++ = nType;
	*p++ = nLen;
	for (uint8_t i = 0; i < nLen; i++)
		*p++ = pVal[i];
}

void CDHCPServer::WriteIP (uint8_t *&p, uint8_t nType, const uint8_t *pIP)
{
	WriteOption (p, nType, pIP, 4);
}

void CDHCPServer::WriteU32 (uint8_t *&p, uint8_t nType, uint32_t nVal)
{
	uint8_t buf[4];
	buf[0] = (uint8_t) (nVal >> 24);
	buf[1] = (uint8_t) (nVal >> 16);
	buf[2] = (uint8_t) (nVal >> 8);
	buf[3] = (uint8_t)  nVal;
	WriteOption (p, nType, buf, 4);
}

size_t CDHCPServer::Process (const uint8_t *pReq, size_t nReqLen,
                             uint8_t *pReply, size_t nReplyMax)
{
	// Basic header validation
	if (nReqLen < DHCP_MIN_SIZE)
		return 0;
	if (pReq[OFF_OP] != BOOTREQUEST)
		return 0;
	if (pReq[OFF_HTYPE] != 1 || pReq[OFF_HLEN] != 6)
		return 0;

	// Magic cookie
	for (unsigned i = 0; i < 4; i++)
		if (pReq[DHCP_MAGIC_OFFSET + i] != s_Magic[i])
			return 0;

	const uint8_t *pMAC  = pReq + OFF_CHADDR;
	const uint8_t *pOpts = pReq + DHCP_OPTIONS_OFFSET;
	size_t nOptsLen = nReqLen - DHCP_OPTIONS_OFFSET;

	uint8_t nMsgType = ParseMsgType (pOpts, nOptsLen);
	if (nMsgType == 0)
		return 0;

	// For REQUEST, verify that the server identifier (if present) matches us
	if (nMsgType == DHCP_REQUEST)
	{
		uint8_t sid[4];
		if (ParseIP (pOpts, nOptsLen, OPT_SERVER_ID, sid))
		{
			for (unsigned i = 0; i < 4; i++)
			{
				if (sid[i] != s_ServerIP[i])
					return 0;	// request is for another server
			}
		}
	}

	// Allocate/find an IP for this client
	uint8_t nSuffix = AllocateSuffix (pMAC);

	// Build the reply
	if (nReplyMax < 300u)
		return 0;

	// Zero-fill the reply
	for (size_t i = 0; i < 300u; i++)
		pReply[i] = 0;

	// Fixed header
	pReply[OFF_OP]    = BOOTREPLY;
	pReply[OFF_HTYPE] = 1;
	pReply[OFF_HLEN]  = 6;
	pReply[OFF_HOPS]  = 0;

	// Copy XID, flags, giaddr from request
	for (unsigned i = 0; i < 4; i++)
		pReply[OFF_XID + i] = pReq[OFF_XID + i];
	for (unsigned i = 0; i < 2; i++)
		pReply[OFF_FLAGS + i] = pReq[OFF_FLAGS + i];
	for (unsigned i = 0; i < 4; i++)
		pReply[OFF_GIADDR + i] = pReq[OFF_GIADDR + i];

	// chaddr — copy MAC
	for (unsigned i = 0; i < 16; i++)
		pReply[OFF_CHADDR + i] = pReq[OFF_CHADDR + i];

	// yiaddr: assigned IP (or 0 on NAK)
	uint8_t nReplyType;
	if (nMsgType == DHCP_DISCOVER)
	{
		nReplyType = (nSuffix == 0) ? 0 : DHCP_OFFER;
	}
	else  // DHCP_REQUEST
	{
		if (nSuffix == 0)
		{
			nReplyType = DHCP_NAK;
		}
		else
		{
			// Verify requested IP matches what we'd offer
			uint8_t reqIP[4];
			if (ParseIP (pOpts, nOptsLen, OPT_REQ_IP, reqIP))
			{
				if (reqIP[0] != 192 || reqIP[1] != 168 ||
				    reqIP[2] != 4   || reqIP[3] != nSuffix)
				{
					nReplyType = DHCP_NAK;
					nSuffix = 0;
				}
				else
				{
					nReplyType = DHCP_ACK;
				}
			}
			else
			{
				// No requested IP — accept ciaddr if it's one of our leases
				const uint8_t *pCI = pReq + OFF_CIADDR;
				if (pCI[0] == 192 && pCI[1] == 168 &&
				    pCI[2] == 4   && pCI[3] == nSuffix)
				{
					nReplyType = DHCP_ACK;
				}
				else
				{
					nReplyType = DHCP_ACK;	// renew/rebind, trust our table
				}
			}
		}
	}

	if (nReplyType == 0)
		return 0;

	// Set yiaddr for OFFER and ACK
	if (nReplyType == DHCP_OFFER || nReplyType == DHCP_ACK)
	{
		pReply[OFF_YIADDR + 0] = 192;
		pReply[OFF_YIADDR + 1] = 168;
		pReply[OFF_YIADDR + 2] = 4;
		pReply[OFF_YIADDR + 3] = nSuffix;
	}

	// Magic cookie
	for (unsigned i = 0; i < 4; i++)
		pReply[DHCP_MAGIC_OFFSET + i] = s_Magic[i];

	// Options
	uint8_t *p = pReply + DHCP_OPTIONS_OFFSET;

	// DHCP message type
	uint8_t nTypeVal = nReplyType;
	WriteOption (p, OPT_MSG_TYPE, &nTypeVal, 1);

	if (nReplyType == DHCP_OFFER || nReplyType == DHCP_ACK)
	{
		WriteIP  (p, OPT_SUBNET,    s_Subnet);
		WriteIP  (p, OPT_ROUTER,    s_ServerIP);
		WriteU32 (p, OPT_LEASE_TIME, DHCP_LEASE_TIME);
	}
	WriteIP (p, OPT_SERVER_ID, s_ServerIP);
	*p++ = OPT_END;

	return (size_t) (p - pReply);
}
