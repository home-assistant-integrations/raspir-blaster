/*
 *  apiserver.cpp
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

#include "apiserver.h"
#include "configwriter.h"
#include "constanttime.h"
#include "formdata.h"
#include "jsonwriter.h"
#include "version.h"
#include "wpareader.h"
#include <circle/logger.h>
#include <circle/net/ipaddress.h>
#include <circle/net/netconfig.h>
#include <circle/sched/scheduler.h>
#include <circle/string.h>
#include <circle/timer.h>
#include <circle/util.h>
#include <fatfs/ff.h>

#define API_MAX_CONTENT_SIZE	4096
#define API_MAX_MULTIPART_SIZE	(8u * 1024u * 1024u + 4096u)	// 8 MB upload + overhead
#define WRONG_PASSWORD_DELAY_MS	1000u

#define DEVICE_CONFIG_PATH	"SD:/raspir-blaster.conf"
#define WPA_CONFIG_PATH		"SD:/wpa_supplicant.conf"
#define CONFIG_FILE_BUF_SIZE	2048

static const char FromApi[] = "api";

// Writes pContent to <pFinalPath>.new, syncs, then replaces pFinalPath with
// it (FatFs cannot rename over an existing file, so the old one is deleted
// first; see design.md D6).
static bool WriteFileAtomic (const char *pFinalPath, const char *pContent, size_t nLen)
{
	char TempPath[80];
	CTextWriter TP (TempPath, sizeof TempPath);
	TP.Append (pFinalPath);
	TP.Append (".new");

	FIL File;
	if (f_open (&File, TempPath, FA_WRITE | FA_CREATE_ALWAYS) != FR_OK)
	{
		return false;
	}

	UINT nWritten = 0;
	FRESULT WriteResult = f_write (&File, pContent, (UINT) nLen, &nWritten);
	f_sync (&File);
	f_close (&File);

	if (WriteResult != FR_OK || nWritten != nLen)
	{
		f_unlink (TempPath);
		return false;
	}

	f_unlink (pFinalPath);	// ignore result: may not exist yet
	if (f_rename (TempPath, pFinalPath) != FR_OK)
	{
		return false;
	}

	return true;
}

static const char *NetworkName (TApiNetwork Network)
{
	switch (Network)
	{
	case ApiNetworkEthernet:    return "ethernet";
	case ApiNetworkWiFi:        return "wifi";
	case ApiNetworkAccessPoint: return "access-point";
	default:                    return "unknown";
	}
}

CApiServer::CApiServer (CNetSubSystem *pNet, const TApiContext *pCtx, CSocket *pSocket)
:	CHTTPDaemon (pNet, pSocket, API_MAX_CONTENT_SIZE, HTTP_PORT, API_MAX_MULTIPART_SIZE),
	m_pCtx (pCtx)
{
}

CApiServer::~CApiServer (void)
{
}

CHTTPDaemon *CApiServer::CreateWorker (CNetSubSystem *pNet, CSocket *pSocket)
{
	return new CApiServer (pNet, m_pCtx, pSocket);
}

THTTPStatus CApiServer::GetContent (const char  *pPath,
                                    const char  *pParams,
                                    const char  *pFormData,
                                    u8          *pBuffer,
                                    unsigned    *pLength,
                                    const char **ppContentType)
{
	*ppContentType = "application/json";

	if (m_pCtx->pLastActivityMs != 0)
	{
		*m_pCtx->pLastActivityMs = NowMs ();
	}

	if (strcmp (pPath, "/api/status") == 0)
	{
		return HandleStatus (pBuffer, pLength);
	}

	if (strcmp (pPath, "/api/update") == 0)
	{
		return HandleUpdate (pBuffer, pLength);
	}

	if (strcmp (pPath, "/api/update/confirm") == 0)
	{
		return HandleUpdateConfirm (pFormData, pBuffer, pLength);
	}

	if (strcmp (pPath, "/api/update/cancel") == 0)
	{
		return HandleUpdateCancel (pFormData, pBuffer, pLength);
	}

	if (strcmp (pPath, "/api/restart") == 0)
	{
		return HandleRestart (pFormData, pBuffer, pLength);
	}

	if (strcmp (pPath, "/api/config") == 0)
	{
		// GetContent() is not told the HTTP method; a GET has no form
		// data (pFormData == ""), a POST does. GET never takes a
		// password: Circle exposes no request headers, and a GET has no
		// body, so the only place to put one would be the query string,
		// which leaks into logs/history/proxies. GET therefore only
		// works in setup mode; a normal-mode read goes through
		// POST /api/config with no raspir-blaster.conf/Wi-Fi fields
		// (password in the form body, like every other endpoint).
		return (pFormData[0] != '\0') ? HandlePostConfig (pFormData, pBuffer, pLength)
		                               : HandleGetConfig (pBuffer, pLength);
	}

	return WriteError (pBuffer, pLength, 404, "not found");
}

THTTPStatus CApiServer::HandleStatus (u8 *pBuf, unsigned *pLen)
{
	CIPAddress IP;
	if (m_pCtx->pNet != 0 && m_pCtx->pNet->GetConfig () != 0
	    && m_pCtx->pNet->GetConfig ()->GetIPAddress () != 0)
	{
		IP = *m_pCtx->pNet->GetConfig ()->GetIPAddress ();
	}
	CString IPString;
	IP.Format (&IPString);

	boolean bUpdateEnabled = m_pCtx->bSetupMode || m_pCtx->pConfig->bHasOTAPassword;
	boolean bMQTTConnected = m_pCtx->pMQTT != 0 && m_pCtx->pMQTT->IsOnline ();

	CJSONWriter J ((char *) pBuf, *pLen);
	J.BeginObject ();
	J.String ("result", "ok");
	J.String ("device_id", m_pCtx->pConfig->DeviceID);
	J.String ("board", m_pCtx->pBoardModel);
	J.String ("version", RASPIR_FIRMWARE_VERSION);
	J.String ("build", m_pCtx->pBuild);
	J.String ("mode", m_pCtx->bSetupMode ? "setup" : "normal");
	J.String ("network", NetworkName (m_pCtx->Network));
	J.String ("ip", (const char *) IPString);
	J.String ("mqtt", bMQTTConnected ? "connected" : "disconnected");
	J.Number ("uptime_s", m_pCtx->pTimer->GetUptime ());
	J.Bool ("update_enabled", bUpdateEnabled);
	J.EndObject ();

	if (J.Overflow ())
	{
		return WriteError (pBuf, pLen, 500, "internal error");
	}

	*pLen = (unsigned) J.GetLength ();
	return HTTPOK;
}

THTTPStatus CApiServer::WriteError (u8 *pBuf, unsigned *pLen,
                                    unsigned nStatus, const char *pMessage)
{
	CJSONWriter J ((char *) pBuf, *pLen);
	J.BeginObject ();
	J.String ("result", "error");
	J.Number ("status", nStatus);
	J.String ("message", pMessage);
	J.EndObject ();

	// Overflow is not expected for these short, fixed-shape error bodies.
	*pLen = J.Overflow () ? 0 : (unsigned) J.GetLength ();
	return HTTPOK;
}

bool CApiServer::CheckOTAPassword (const char *pPassword, u8 *pBuf, unsigned *pLen,
                                   THTTPStatus *pResult)
{
	if (m_pCtx->bSetupMode)
	{
		return true;	// no password required in setup mode
	}

	if (!m_pCtx->pConfig->bHasOTAPassword)
	{
		*pResult = WriteError (pBuf, pLen, 403, "updates disabled");
		return false;
	}

	if (pPassword != 0 && ConstantTimeEqual (pPassword, m_pCtx->pConfig->OTAPassword))
	{
		return true;
	}

	// Never log the entered value, only that an attempt failed.
	CLogger::Get ()->Write (FromApi, LogWarning, "Wrong password for update/restart");
	CScheduler::Get ()->MsSleep (WRONG_PASSWORD_DELAY_MS);

	*pResult = WriteError (pBuf, pLen, 403, "wrong password");
	return false;
}

u64 CApiServer::NowMs (void)
{
	return CTimer::GetClockTicks64 () / 1000;
}

const char *CApiServer::ReasonString (TVersionOrder Reason)
{
	switch (Reason)
	{
	case VersionSame:          return "same";
	case VersionOlder:         return "older";
	case VersionNotComparable: return "not_comparable";
	default:                   return "not_comparable";
	}
}

THTTPStatus CApiServer::HandleUpdate (u8 *pBuf, unsigned *pLen)
{
	if (m_pCtx->pUpdater->IsBusy ())
	{
		return WriteError (pBuf, pLen, 409, "update already in progress");
	}

	char Password[CONFIG_OTA_PASSWORD_MAX + 1];
	boolean bHavePassword = FALSE;

	const uint8_t *pZipData = 0;
	unsigned nZipLen = 0;
	boolean bHaveFile = FALSE;

	const char *pPartHeader;
	const u8 *pPartData;
	unsigned nPartLength;
	while (GetMultipartFormPart (&pPartHeader, &pPartData, &nPartLength))
	{
		if (strstr (pPartHeader, "name=\"file\"") != 0)
		{
			pZipData = pPartData;
			nZipLen = nPartLength;
			bHaveFile = TRUE;
		}
		else if (strstr (pPartHeader, "name=\"password\"") != 0)
		{
			unsigned n = nPartLength < sizeof Password - 1 ? nPartLength : sizeof Password - 1;
			memcpy (Password, pPartData, n);
			Password[n] = '\0';
			bHavePassword = TRUE;
		}
	}

	THTTPStatus Result;
	if (!CheckOTAPassword (bHavePassword ? Password : 0, pBuf, pLen, &Result))
	{
		return Result;
	}

	if (!bHaveFile || nZipLen == 0)
	{
		return WriteError (pBuf, pLen, 400, "missing file");
	}

	TFirmwareValidateResult VR = m_pCtx->pUpdater->Validate (pZipData, nZipLen, NowMs ());

	if (VR.Outcome == FirmwareUpdateError)
	{
		return WriteError (pBuf, pLen, 400, VR.ErrorMessage);
	}

	CJSONWriter J ((char *) pBuf, *pLen);
	J.BeginObject ();
	if (VR.Outcome == FirmwareUpdateInstalled)
	{
		J.String ("result", "installed");
	}
	else
	{
		J.String ("result", "confirm_required");
		J.String ("installed_version", VR.InstalledVersion);
		J.String ("uploaded_version", VR.UploadedVersion);
		J.String ("reason", ReasonString (VR.Reason));
		J.String ("token", VR.Token);
	}
	J.EndObject ();

	*pLen = J.Overflow () ? 0 : (unsigned) J.GetLength ();
	return HTTPOK;
}

THTTPStatus CApiServer::HandleUpdateConfirm (const char *pFormData, u8 *pBuf, unsigned *pLen)
{
	char Password[CONFIG_OTA_PASSWORD_MAX + 1];
	TFormResult PasswordResult = FormDataGet (pFormData, "password", Password, sizeof Password);

	THTTPStatus Result;
	if (!CheckOTAPassword (PasswordResult == FormFound ? Password : 0, pBuf, pLen, &Result))
	{
		return Result;
	}

	char Token[FW_UPDATE_TOKEN_LEN + 1];
	if (FormDataGet (pFormData, "token", Token, sizeof Token) != FormFound)
	{
		return WriteError (pBuf, pLen, 400, "missing token");
	}

	const char *pError = m_pCtx->pUpdater->Confirm (Token, NowMs ());
	if (pError != 0)
	{
		return WriteError (pBuf, pLen, 400, pError);
	}

	CJSONWriter J ((char *) pBuf, *pLen);
	J.BeginObject ();
	J.String ("result", "installed");
	J.EndObject ();

	*pLen = J.Overflow () ? 0 : (unsigned) J.GetLength ();
	return HTTPOK;
}

THTTPStatus CApiServer::HandleUpdateCancel (const char *pFormData, u8 *pBuf, unsigned *pLen)
{
	char Password[CONFIG_OTA_PASSWORD_MAX + 1];
	TFormResult PasswordResult = FormDataGet (pFormData, "password", Password, sizeof Password);

	THTTPStatus Result;
	if (!CheckOTAPassword (PasswordResult == FormFound ? Password : 0, pBuf, pLen, &Result))
	{
		return Result;
	}

	if (!m_pCtx->pUpdater->Cancel ())
	{
		return WriteError (pBuf, pLen, 400, "no update pending");
	}

	CJSONWriter J ((char *) pBuf, *pLen);
	J.BeginObject ();
	J.String ("result", "cancelled");
	J.EndObject ();

	*pLen = J.Overflow () ? 0 : (unsigned) J.GetLength ();
	return HTTPOK;
}

THTTPStatus CApiServer::HandleRestart (const char *pFormData, u8 *pBuf, unsigned *pLen)
{
	char Password[CONFIG_OTA_PASSWORD_MAX + 1];
	TFormResult PasswordResult = FormDataGet (pFormData, "password", Password, sizeof Password);

	THTTPStatus Result;
	if (!CheckOTAPassword (PasswordResult == FormFound ? Password : 0, pBuf, pLen, &Result))
	{
		return Result;
	}

	m_pCtx->pUpdater->ScheduleRestart (NowMs ());

	CJSONWriter J ((char *) pBuf, *pLen);
	J.BeginObject ();
	J.String ("result", "restarting");
	J.EndObject ();

	*pLen = J.Overflow () ? 0 : (unsigned) J.GetLength ();
	return HTTPOK;
}

THTTPStatus CApiServer::HandleGetConfig (u8 *pBuf, unsigned *pLen)
{
	// GET never takes a password (see GetContent()'s dispatch comment),
	// so it only works where none is required.
	if (!m_pCtx->bSetupMode)
	{
		return WriteError (pBuf, pLen, 403,
		                   "use POST /api/config with only 'password' to read in normal mode");
	}

	return WriteConfigJSON (pBuf, pLen);
}

// Shared by HandleGetConfig() (setup mode) and HandlePostConfig()'s
// read-only case (normal mode, password already checked by the caller).
THTTPStatus CApiServer::WriteConfigJSON (u8 *pBuf, unsigned *pLen)
{
	TWiFiStatus WiFi;
	WiFi.bConfigured = false;
	WiFi.bPasswordSet = false;
	WiFi.SSID[0] = '\0';
	WiFi.Country[0] = '\0';

	FIL File;
	if (f_open (&File, WPA_CONFIG_PATH, FA_READ | FA_OPEN_EXISTING) == FR_OK)
	{
		static char s_WpaBuf[CONFIG_FILE_BUF_SIZE];
		UINT nRead = 0;
		if (f_read (&File, s_WpaBuf, sizeof s_WpaBuf - 1, &nRead) == FR_OK)
		{
			s_WpaBuf[nRead] = '\0';
			WPAConfigRead (s_WpaBuf, nRead, &WiFi);
		}
		f_close (&File);
	}

	const TConfig *pC = m_pCtx->pConfig;

	CJSONWriter J ((char *) pBuf, *pLen);
	J.BeginObject ();
	J.String ("result", "ok");
	J.String ("mqtt_host", pC->MQTTHost);
	J.Number ("mqtt_port", pC->nMQTTPort);
	J.Bool ("mqtt_user_set", pC->bHasUser);
	if (pC->bHasUser)
	{
		J.String ("mqtt_user", pC->MQTTUser);
	}
	J.Bool ("mqtt_password_set", pC->bHasPassword);
	J.String ("device_id", pC->DeviceID);
	J.String ("topic_base", pC->TopicBase);
	J.Number ("ir_frame_gap_ms", pC->nIRFrameGapMs);
	J.Bool ("ota_password_set", pC->bHasOTAPassword);
	J.String ("wifi_ssid", WiFi.SSID);
	J.String ("wifi_country", WiFi.Country);
	J.Bool ("wifi_password_set", WiFi.bPasswordSet);
	J.EndObject ();

	if (J.Overflow ())
	{
		return WriteError (pBuf, pLen, 500, "internal error");
	}

	*pLen = (unsigned) J.GetLength ();
	return HTTPOK;
}

THTTPStatus CApiServer::HandlePostConfig (const char *pFormData, u8 *pBuf, unsigned *pLen)
{
	char Password[CONFIG_OTA_PASSWORD_MAX + 1];
	TFormResult PasswordResult = FormDataGet (pFormData, "password", Password, sizeof Password);

	THTTPStatus Result;
	if (!CheckOTAPassword (PasswordResult == FormFound ? Password : 0, pBuf, pLen, &Result))
	{
		return Result;
	}

	static const char *s_ConfKeys[] = {
		"mqtt_host", "mqtt_port", "mqtt_user", "mqtt_password",
		"device_id", "topic_base", "ir_frame_gap_ms", "ota_password",
	};
	static const unsigned nConfKeys = sizeof s_ConfKeys / sizeof s_ConfKeys[0];

	// No raspir-blaster.conf/Wi-Fi field given at all: this is a
	// password-authenticated read, not a write (the normal-mode
	// counterpart of GET /api/config, which cannot carry a password
	// safely). Changes nothing.
	{
		char Dummy[2];
		// A too-long or invalid value still means the key is present in
		// the request; only FormMissing means it is truly absent.
		bool bAnyFieldGiven = FormDataGet (pFormData, "wifi_ssid", Dummy, sizeof Dummy) != FormMissing;
		for (unsigned i = 0; i < nConfKeys && !bAnyFieldGiven; i++)
		{
			bAnyFieldGiven = FormDataGet (pFormData, s_ConfKeys[i], Dummy, sizeof Dummy) != FormMissing;
		}
		if (!bAnyFieldGiven)
		{
			return WriteConfigJSON (pBuf, pLen);
		}
	}

	TConfig NewConfig;
	ConfigSetDefaults (&NewConfig);

	const char *pInvalidFields[nConfKeys + 1];	// +1 for wifi_country
	unsigned nInvalidFields = 0;

	for (unsigned i = 0; i < nConfKeys; i++)
	{
		char Value[CONFIG_PASSWORD_MAX + 1];	// large enough for every key above
		if (   FormDataGet (pFormData, s_ConfKeys[i], Value, sizeof Value) == FormFound
		    && !ConfigValidateField (s_ConfKeys[i], Value, &NewConfig))
		{
			pInvalidFields[nInvalidFields++] = s_ConfKeys[i];
		}
	}

	TWiFiConfig NewWiFi;
	NewWiFi.SSID[0] = '\0';
	NewWiFi.PSK[0] = '\0';
	NewWiFi.Country[0] = '\0';
	boolean bHaveWiFi = FALSE;

	char SSID[CONFIGWRITER_WIFI_SSID_MAX + 1];
	if (FormDataGet (pFormData, "wifi_ssid", SSID, sizeof SSID) == FormFound)
	{
		bHaveWiFi = TRUE;
		strncpy (NewWiFi.SSID, SSID, sizeof NewWiFi.SSID - 1);
		NewWiFi.SSID[sizeof NewWiFi.SSID - 1] = '\0';

		char Password[CONFIGWRITER_WIFI_PSK_MAX + 1];
		if (FormDataGet (pFormData, "wifi_password", Password, sizeof Password) == FormFound)
		{
			strncpy (NewWiFi.PSK, Password, sizeof NewWiFi.PSK - 1);
			NewWiFi.PSK[sizeof NewWiFi.PSK - 1] = '\0';
		}

		char Country[CONFIGWRITER_WIFI_COUNTRY_MAX + 1];
		if (   FormDataGet (pFormData, "wifi_country", Country, sizeof Country) != FormFound
		    || Country[0] < 'A' || Country[0] > 'Z'
		    || Country[1] < 'A' || Country[1] > 'Z'
		    || Country[2] != '\0')
		{
			pInvalidFields[nInvalidFields++] = "wifi_country";
		}
		else
		{
			NewWiFi.Country[0] = Country[0];
			NewWiFi.Country[1] = Country[1];
			NewWiFi.Country[2] = '\0';
		}
	}

	if (nInvalidFields > 0)
	{
		CJSONWriter J ((char *) pBuf, *pLen);
		J.BeginObject ();
		J.String ("result", "error");
		J.Number ("status", 400);
		J.String ("message", "invalid field value");
		J.BeginArray ("fields");
		for (unsigned i = 0; i < nInvalidFields; i++)
		{
			J.String (0, pInvalidFields[i]);
		}
		J.EndArray ();
		J.EndObject ();

		*pLen = J.Overflow () ? 0 : (unsigned) J.GetLength ();
		return HTTPOK;
	}

	char ConfBuf[CONFIG_FILE_BUF_SIZE];
	if (   ConfigWriteConf (NewConfig, ConfBuf, sizeof ConfBuf) != ConfigWriteOK
	    || !WriteFileAtomic (DEVICE_CONFIG_PATH, ConfBuf, strlen (ConfBuf)))
	{
		return WriteError (pBuf, pLen, 500, "cannot write raspir-blaster.conf");
	}

	if (bHaveWiFi)
	{
		char WpaBuf[CONFIG_FILE_BUF_SIZE];
		if (   ConfigWriteWPA (NewWiFi, WpaBuf, sizeof WpaBuf) != ConfigWriteOK
		    || !WriteFileAtomic (WPA_CONFIG_PATH, WpaBuf, strlen (WpaBuf)))
		{
			return WriteError (pBuf, pLen, 500, "cannot write wpa_supplicant.conf");
		}
	}

	CLogger::Get ()->Write (FromApi, LogNotice, "Configuration saved via POST /api/config");

	CJSONWriter J ((char *) pBuf, *pLen);
	J.BeginObject ();
	J.String ("result", "saved");
	J.EndObject ();

	*pLen = J.Overflow () ? 0 : (unsigned) J.GetLength ();
	return HTTPOK;
}
