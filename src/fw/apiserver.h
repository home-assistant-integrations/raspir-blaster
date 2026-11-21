/*
 *  apiserver.h
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

#ifndef _raspir_apiserver_h
#define _raspir_apiserver_h

#include "config.h"
#include "firmwareupdater.h"
#include "irmqttclient.h"
#include <circle/net/httpdaemon.h>
#include <circle/net/netsubsystem.h>
#include <circle/timer.h>
#include <circle/types.h>

// Network type for GET /api/status "network" field.
enum TApiNetwork
{
	ApiNetworkEthernet,
	ApiNetworkWiFi,
	ApiNetworkAccessPoint,
};

// Shared context visible to both the listener and all worker tasks.
// Owned by whoever creates the listener.
struct TApiContext
{
	const TConfig     *pConfig;
	CNetSubSystem     *pNet;
	CIRMqttClient     *pMQTT;		// 0 in setup mode
	CTimer            *pTimer;
	CFirmwareUpdater  *pUpdater;
	const char        *pBoardModel;		// e.g. "Raspberry Pi Zero W"
	const char        *pBuild;		// "sdcard" or "cm4-emmc"
	boolean            bSetupMode;
	TApiNetwork        Network;
	u64               *pLastActivityMs;	// setup mode only: written on every request, 0 otherwise
};

class CApiServer : public CHTTPDaemon
{
public:
	// Listener constructor (pSocket == 0, created by the kernel).
	CApiServer (CNetSubSystem *pNet, const TApiContext *pCtx,
	            CSocket *pSocket = 0);

	~CApiServer (void);

	CHTTPDaemon *CreateWorker (CNetSubSystem *pNet, CSocket *pSocket) override;

	THTTPStatus GetContent (const char  *pPath,
	                        const char  *pParams,
	                        const char  *pFormData,
	                        u8          *pBuffer,
	                        unsigned    *pLength,
	                        const char **ppContentType) override;

private:
	THTTPStatus HandleStatus        (u8 *pBuf, unsigned *pLen);
	THTTPStatus HandleUpdate        (u8 *pBuf, unsigned *pLen);
	THTTPStatus HandleUpdateConfirm (const char *pFormData, u8 *pBuf, unsigned *pLen);
	THTTPStatus HandleUpdateCancel  (const char *pFormData, u8 *pBuf, unsigned *pLen);
	THTTPStatus HandleRestart       (const char *pFormData, u8 *pBuf, unsigned *pLen);
	THTTPStatus HandleGetConfig     (const char *pParams, u8 *pBuf, unsigned *pLen);
	THTTPStatus HandlePostConfig    (const char *pFormData, u8 *pBuf, unsigned *pLen);

	// Writes {"result":"error","status":nStatus,"message":pMessage} and
	// always returns HTTPOK: see design.md D2 for why the wire status is
	// always 200 (Circle's CHTTPDaemon substitutes an HTML body for any
	// other status, with no override point).
	static THTTPStatus WriteError (u8 *pBuf, unsigned *pLen,
	                               unsigned nStatus, const char *pMessage);

	// In normal mode: requires pPassword (0 if the field was absent) to
	// equal ota_password (constant-time). Without ota_password
	// configured, or on a wrong password (after a 1 s delay, logged
	// without the entered value), writes the 403 error and returns
	// false; the caller must then return *pResult from GetContent().
	// In setup mode this always returns true (no password needed).
	bool CheckOTAPassword (const char *pPassword, u8 *pBuf, unsigned *pLen,
	                       THTTPStatus *pResult);

	static const char *ReasonString (TVersionOrder Reason);
	static u64 NowMs (void);

	const TApiContext *m_pCtx;
};

#endif
