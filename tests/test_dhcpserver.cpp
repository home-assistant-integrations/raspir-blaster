/*
 *  test_dhcpserver.cpp
 *  raspir-blaster tests
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

#include "test.h"
#include "dhcpserver.h"
#include <cstring>
#include <vector>

// ----- packet builder helpers ---------------------------------------------------

namespace
{
	// Fixed DHCP header offsets (RFC 2131)
	enum {
		OFF_OP     = 0,
		OFF_HTYPE  = 1,
		OFF_HLEN   = 2,
		OFF_HOPS   = 3,
		OFF_XID    = 4,
		OFF_SECS   = 8,
		OFF_FLAGS  = 10,
		OFF_CIADDR = 12,
		OFF_YIADDR = 16,
		OFF_SIADDR = 20,
		OFF_GIADDR = 24,
		OFF_CHADDR = 28,
	};

	static const uint8_t s_ServerIP[4] = {192, 168, 4, 1};

	// Build a minimal DHCP BOOTREQUEST packet into buf (must be >= 300 bytes).
	// Returns packet length.
	size_t MakeDHCP (uint8_t *buf, size_t nBufSize,
	                 uint8_t nMsgType, const uint8_t *pMAC, uint32_t nXID,
	                 const uint8_t *pReqIP = 0, const uint8_t *pServerID = 0,
	                 uint32_t nCIAddr = 0)
	{
		std::memset (buf, 0, nBufSize < 300u ? nBufSize : 300u);
		buf[OFF_OP]    = 1;	// BOOTREQUEST
		buf[OFF_HTYPE] = 1;
		buf[OFF_HLEN]  = 6;
		// XID big-endian
		buf[OFF_XID + 0] = (uint8_t)(nXID >> 24);
		buf[OFF_XID + 1] = (uint8_t)(nXID >> 16);
		buf[OFF_XID + 2] = (uint8_t)(nXID >> 8);
		buf[OFF_XID + 3] = (uint8_t)(nXID);
		// broadcast flag
		buf[OFF_FLAGS] = 0x80;
		// ciaddr
		buf[OFF_CIADDR + 0] = (uint8_t)(nCIAddr >> 24);
		buf[OFF_CIADDR + 1] = (uint8_t)(nCIAddr >> 16);
		buf[OFF_CIADDR + 2] = (uint8_t)(nCIAddr >> 8);
		buf[OFF_CIADDR + 3] = (uint8_t)(nCIAddr);
		// chaddr
		std::memcpy (buf + OFF_CHADDR, pMAC, 6);
		// magic cookie
		buf[236] = 0x63; buf[237] = 0x82; buf[238] = 0x53; buf[239] = 0x63;
		// options
		uint8_t *p = buf + 240;
		*p++ = 53; *p++ = 1; *p++ = nMsgType;
		if (pReqIP)
		{
			*p++ = 50; *p++ = 4;
			p[0] = pReqIP[0]; p[1] = pReqIP[1]; p[2] = pReqIP[2]; p[3] = pReqIP[3];
			p += 4;
		}
		if (pServerID)
		{
			*p++ = 54; *p++ = 4;
			p[0] = pServerID[0]; p[1] = pServerID[1];
			p[2] = pServerID[2]; p[3] = pServerID[3];
			p += 4;
		}
		*p++ = 255;	// END
		return (size_t)(p - buf);
	}

	// Extract message type from reply options.
	uint8_t ReplyMsgType (const uint8_t *pReply, size_t nLen)
	{
		if (nLen < 243u) return 0;
		const uint8_t *p = pReply + 240;
		const uint8_t *end = pReply + nLen;
		while (p < end)
		{
			uint8_t t = *p++;
			if (t == 255) break;
			if (t == 0)   continue;
			if (p >= end) break;
			uint8_t l = *p++;
			if (t == 53 && l >= 1) return *p;
			p += l;
		}
		return 0;
	}

	// Extract yiaddr from reply.
	void ReplyYIAddr (const uint8_t *pReply, uint8_t *pIP)
	{
		pIP[0] = pReply[OFF_YIADDR + 0];
		pIP[1] = pReply[OFF_YIADDR + 1];
		pIP[2] = pReply[OFF_YIADDR + 2];
		pIP[3] = pReply[OFF_YIADDR + 3];
	}

	// Check XID match between request and reply.
	bool XIDMatch (const uint8_t *pReq, const uint8_t *pReply)
	{
		for (unsigned i = 0; i < 4; i++)
			if (pReq[OFF_XID + i] != pReply[OFF_XID + i]) return false;
		return true;
	}

	const uint8_t s_MACMacOS[6]   = {0x00, 0x25, 0x96, 0xAA, 0xBB, 0xCC};
	const uint8_t s_MACiOS[6]     = {0xBC, 0xE9, 0x2F, 0x11, 0x22, 0x33};
	const uint8_t s_MACAndroid[6] = {0xC4, 0x0B, 0xCB, 0x44, 0x55, 0x66};
}

// ----- tests --------------------------------------------------------------------

TEST (DHCPDiscoverMacOSGetsOffer)
{
	CDHCPServer srv;
	uint8_t req[300], reply[512];
	std::memset (req, 0, sizeof req);
	std::memset (reply, 0, sizeof reply);

	size_t reqLen = MakeDHCP (req, sizeof req, 1 /*DISCOVER*/, s_MACMacOS, 0xAABBCC01);
	size_t replyLen = srv.Process (req, reqLen, reply, sizeof reply);

	CHECK (replyLen > 0u);
	CHECK_EQ ((int) ReplyMsgType (reply, replyLen), 2 /*OFFER*/);
	CHECK (XIDMatch (req, reply));

	uint8_t yip[4];
	ReplyYIAddr (reply, yip);
	CHECK_EQ (yip[0], 192u);
	CHECK_EQ (yip[1], 168u);
	CHECK_EQ (yip[2], 4u);
	CHECK (yip[3] >= 10u && yip[3] <= 50u);
}

TEST (DHCPDiscoverIOSGetsOffer)
{
	CDHCPServer srv;
	uint8_t req[300], reply[512];
	size_t reqLen = MakeDHCP (req, sizeof req, 1, s_MACiOS, 0xAABBCC02);
	size_t replyLen = srv.Process (req, reqLen, reply, sizeof reply);

	CHECK (replyLen > 0u);
	CHECK_EQ ((int) ReplyMsgType (reply, replyLen), 2);
}

TEST (DHCPDiscoverAndroidGetsOffer)
{
	CDHCPServer srv;
	uint8_t req[300], reply[512];
	size_t reqLen = MakeDHCP (req, sizeof req, 1, s_MACAndroid, 0xAABBCC03);
	size_t replyLen = srv.Process (req, reqLen, reply, sizeof reply);

	CHECK (replyLen > 0u);
	CHECK_EQ ((int) ReplyMsgType (reply, replyLen), 2);
}

TEST (DHCPRequestAfterOfferGetsACK)
{
	CDHCPServer srv;
	uint8_t req[300], reply[512];

	// First: DISCOVER
	size_t reqLen = MakeDHCP (req, sizeof req, 1, s_MACMacOS, 0x11111111);
	srv.Process (req, reqLen, reply, sizeof reply);

	// Extract offered IP
	uint8_t offeredIP[4];
	ReplyYIAddr (reply, offeredIP);

	// Second: REQUEST for the offered IP, citing our server
	reqLen = MakeDHCP (req, sizeof req, 3 /*REQUEST*/, s_MACMacOS, 0x11111111,
	                   offeredIP, s_ServerIP);
	size_t replyLen = srv.Process (req, reqLen, reply, sizeof reply);

	CHECK (replyLen > 0u);
	CHECK_EQ ((int) ReplyMsgType (reply, replyLen), 5 /*ACK*/);

	uint8_t ackedIP[4];
	ReplyYIAddr (reply, ackedIP);
	CHECK_EQ (ackedIP[0], offeredIP[0]);
	CHECK_EQ (ackedIP[1], offeredIP[1]);
	CHECK_EQ (ackedIP[2], offeredIP[2]);
	CHECK_EQ (ackedIP[3], offeredIP[3]);
}

TEST (DHCPRequestWrongIPGetsNAK)
{
	CDHCPServer srv;
	uint8_t req[300], reply[512];

	// DISCOVER to allocate a slot
	size_t reqLen = MakeDHCP (req, sizeof req, 1, s_MACMacOS, 0x22222222);
	srv.Process (req, reqLen, reply, sizeof reply);

	uint8_t offeredIP[4];
	ReplyYIAddr (reply, offeredIP);

	// REQUEST a different IP (last octet off by 1)
	uint8_t wrongIP[4] = {192, 168, 4, (uint8_t)(offeredIP[3] + 1u)};
	reqLen = MakeDHCP (req, sizeof req, 3, s_MACMacOS, 0x22222222,
	                   wrongIP, s_ServerIP);
	size_t replyLen = srv.Process (req, reqLen, reply, sizeof reply);

	CHECK (replyLen > 0u);
	CHECK_EQ ((int) ReplyMsgType (reply, replyLen), 6 /*NAK*/);
}

TEST (DHCPSameMACGetsSameIP)
{
	CDHCPServer srv;
	uint8_t req[300], reply[512];

	// First DISCOVER
	size_t reqLen = MakeDHCP (req, sizeof req, 1, s_MACiOS, 0x33333333);
	srv.Process (req, reqLen, reply, sizeof reply);
	uint8_t firstIP[4];
	ReplyYIAddr (reply, firstIP);

	// Second DISCOVER same MAC (simulates renew attempt via new DISCOVER)
	reqLen = MakeDHCP (req, sizeof req, 1, s_MACiOS, 0x44444444);
	srv.Process (req, reqLen, reply, sizeof reply);
	uint8_t secondIP[4];
	ReplyYIAddr (reply, secondIP);

	CHECK_EQ (secondIP[3], firstIP[3]);
}

TEST (DHCPThreeClientsGetDifferentIPs)
{
	CDHCPServer srv;
	uint8_t req[300], reply[512];

	size_t r1 = MakeDHCP (req, sizeof req, 1, s_MACMacOS, 0xA1);
	srv.Process (req, r1, reply, sizeof reply);
	uint8_t ip1[4]; ReplyYIAddr (reply, ip1);

	size_t r2 = MakeDHCP (req, sizeof req, 1, s_MACiOS, 0xA2);
	srv.Process (req, r2, reply, sizeof reply);
	uint8_t ip2[4]; ReplyYIAddr (reply, ip2);

	size_t r3 = MakeDHCP (req, sizeof req, 1, s_MACAndroid, 0xA3);
	srv.Process (req, r3, reply, sizeof reply);
	uint8_t ip3[4]; ReplyYIAddr (reply, ip3);

	CHECK (ip1[3] != ip2[3]);
	CHECK (ip1[3] != ip3[3]);
	CHECK (ip2[3] != ip3[3]);
	CHECK_EQ (srv.GetLeaseCount (), 3u);
}

TEST (DHCPPoolExhaustionReturnsNoReply)
{
	CDHCPServer srv;
	uint8_t req[300], reply[512];

	// Fill all 41 slots with distinct MACs
	for (unsigned i = 0; i < 41u; i++)
	{
		uint8_t mac[6] = {0xDE, 0xAD, 0, 0, 0, (uint8_t) i};
		size_t reqLen = MakeDHCP (req, sizeof req, 1, mac, (uint32_t)(0xBB000000 + i));
		size_t replyLen = srv.Process (req, reqLen, reply, sizeof reply);
		CHECK (replyLen > 0u);	// all should succeed
	}
	CHECK_EQ (srv.GetLeaseCount (), 41u);

	// 42nd client gets no reply
	uint8_t newMAC[6] = {0xDE, 0xAD, 0xBE, 0xEF, 0xCA, 0xFE};
	size_t reqLen = MakeDHCP (req, sizeof req, 1, newMAC, 0xFFFFFFFF);
	size_t replyLen = srv.Process (req, reqLen, reply, sizeof reply);
	CHECK_EQ (replyLen, (size_t) 0);
}

TEST (DHCPRequestForOtherServerIsIgnored)
{
	CDHCPServer srv;
	uint8_t req[300], reply[512];

	uint8_t otherServer[4] = {10, 0, 0, 1};
	uint8_t offeredIP[4]   = {10, 0, 0, 100};
	size_t reqLen = MakeDHCP (req, sizeof req, 3, s_MACMacOS, 0x55555555,
	                          offeredIP, otherServer);
	size_t replyLen = srv.Process (req, reqLen, reply, sizeof reply);

	CHECK_EQ (replyLen, (size_t) 0);
}

TEST (DHCPMalformedPacketIsIgnored)
{
	CDHCPServer srv;
	uint8_t req[200], reply[512];
	std::memset (req, 0, sizeof req);

	// Too short
	CHECK_EQ (srv.Process (req, 10, reply, sizeof reply), (size_t) 0);

	// Not BOOTREQUEST
	uint8_t badReq[300];
	MakeDHCP (badReq, sizeof badReq, 1, s_MACMacOS, 0x11);
	badReq[0] = 2;	// BOOTREPLY
	CHECK_EQ (srv.Process (badReq, 300, reply, sizeof reply), (size_t) 0);
}

TEST (DHCPOfferContainsServerIDAndSubnet)
{
	CDHCPServer srv;
	uint8_t req[300], reply[512];
	size_t reqLen = MakeDHCP (req, sizeof req, 1, s_MACMacOS, 0x66666666);
	size_t replyLen = srv.Process (req, reqLen, reply, sizeof reply);
	CHECK (replyLen > 0u);

	// Scan options for server identifier (54) and subnet mask (1)
	bool hasServerID = false, hasSubnet = false;
	const uint8_t *p = reply + 240;
	const uint8_t *end = reply + replyLen;
	while (p < end)
	{
		uint8_t t = *p++;
		if (t == 255) break;
		if (t == 0)   continue;
		if (p >= end) break;
		uint8_t l = *p++;
		if (t == 54 && l >= 4)
		{
			hasServerID = (p[0] == 192 && p[1] == 168 && p[2] == 4 && p[3] == 1);
		}
		if (t == 1 && l >= 4)
		{
			hasSubnet = (p[0] == 255 && p[1] == 255 && p[2] == 255 && p[3] == 0);
		}
		p += l;
	}
	CHECK (hasServerID);
	CHECK (hasSubnet);
}

TEST (DHCPTruncatedOptionDoesNotReadOutOfBounds)
{
	// A SERVER_ID (type 54) option claiming length 4 with only 2 bytes
	// actually present before the buffer ends. ParseIP() must not read
	// past the declared options length. The request buffer is sized
	// exactly to its content (no slack) so ASan/UBSan catch any overread.
	std::vector<uint8_t> req (240 + 7, 0);
	req[OFF_OP] = 1; req[OFF_HTYPE] = 1; req[OFF_HLEN] = 6;
	req[236] = 0x63; req[237] = 0x82; req[238] = 0x53; req[239] = 0x63;
	req[240] = 53; req[241] = 1; req[242] = 3;	// DHCP message type: REQUEST
	req[243] = 54; req[244] = 4;			// SERVER_ID, claims 4 bytes...
	req[245] = 192; req[246] = 168;		// ...but only 2 are present

	CDHCPServer srv;
	uint8_t reply[512];
	size_t replyLen = srv.Process (req.data (), req.size (), reply, sizeof reply);

	// No crash/OOB read (checked by the sanitizers); the truncated option
	// is simply ignored, same as if it were absent.
	(void) replyLen;
}
