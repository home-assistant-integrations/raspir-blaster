/*
 *  config.cpp
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

#include "config.h"
#include "textwriter.h"

namespace
{
	/// A slice of the input text (not NUL-terminated).
	struct TSlice
	{
		const char *p;
		size_t      n;
	};

	bool IsSpace (char ch)
	{
		return ch == ' ' || ch == '\t' || ch == '\r';
	}

	TSlice Trim (TSlice s)
	{
		while (s.n > 0 && IsSpace (s.p[0]))
		{
			s.p++;
			s.n--;
		}

		while (s.n > 0 && IsSpace (s.p[s.n - 1]))
		{
			s.n--;
		}

		return s;
	}

	typedef bool TCharPredicate (char ch);

	bool IsHostChar (char ch)
	{
		return    (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z')
		       || (ch >= '0' && ch <= '9') || ch == '.' || ch == '-';
	}

	bool IsDeviceIDChar (char ch)
	{
		return (ch >= 'a' && ch <= 'z') || (ch >= '0' && ch <= '9') || ch == '_';
	}

	bool IsTopicChar (char ch)
	{
		return ch > ' ' && ch != 0x7F && ch != '+' && ch != '#';
	}

	bool IsCredentialChar (char ch)
	{
		return ch >= ' ' && ch != 0x7F;
	}

	/// Copies s into Dest if it has 1..nMax characters, all accepted by pPredicate.
	bool CopyString (TSlice s, size_t nMax, TCharPredicate *pPredicate, char *pDest)
	{
		if (s.n == 0 || s.n > nMax)
		{
			return false;
		}

		for (size_t i = 0; i < s.n; i++)
		{
			if (!pPredicate (s.p[i]))
			{
				return false;
			}
		}

		for (size_t i = 0; i < s.n; i++)
		{
			pDest[i] = s.p[i];
		}
		pDest[s.n] = '\0';

		return true;
	}

	bool ParseUnsigned (TSlice s, unsigned nMin, unsigned nMax, unsigned *pValue)
	{
		if (s.n == 0 || s.n > 5)
		{
			return false;
		}

		unsigned nValue = 0;
		for (size_t i = 0; i < s.n; i++)
		{
			if (s.p[i] < '0' || s.p[i] > '9')
			{
				return false;
			}

			nValue = nValue * 10 + (unsigned) (s.p[i] - '0');
		}

		if (nValue < nMin || nValue > nMax)
		{
			return false;
		}

		*pValue = nValue;
		return true;
	}

	void CopyCString (const char *pSource, char *pDest)
	{
		while ((*pDest++ = *pSource++) != '\0')
		{
		}
	}

	class CMessenger
	{
	public:
		CMessenger (TConfigMessageHandler *pHandler, void *pParam)
		:	m_pHandler (pHandler), m_pParam (pParam) {}

		void Report (unsigned nLine, const char *pText1, TSlice Detail,
			     const char *pText2, const char *pText3 = "")
		{
			char Buffer[160];
			CTextWriter Writer (Buffer, sizeof Buffer);
			if (nLine > 0)
			{
				Writer.Append ("line ");
				Writer.AppendUnsigned (nLine);
				Writer.Append (": ");
			}
			Writer.Append (pText1);
			Writer.Append (Detail.p, Detail.n);
			Writer.Append (pText2);
			Writer.Append (pText3);
			Send (Buffer);
		}

		void Send (const char *pMessage)
		{
			if (m_pHandler != 0)
			{
				m_pHandler (m_pParam, pMessage);
			}
		}

	private:
		TConfigMessageHandler *m_pHandler;
		void                  *m_pParam;
	};

	const TSlice NoDetail = { "", 0 };

	void InvalidValue (CMessenger &Messenger, unsigned nLine, const char *pKey, TSlice Value)
	{
		char Key[48];
		CTextWriter Writer (Key, sizeof Key);
		Writer.Append ("' for '");
		Writer.Append (pKey);
		Writer.Append ("'");
		Messenger.Report (nLine, "invalid value '", Value, Key, " - using default");
	}

	void ApplyKey (TConfig *pConfig, TSlice Key, TSlice Value, unsigned nLine,
		       CMessenger &Messenger, bool *pPasswordSeen)
	{
		unsigned nValue;

		if (CoreStrEqualN (Key.p, Key.n, "mqtt_host"))
		{
			if (!CopyString (Value, CONFIG_HOST_MAX, IsHostChar, pConfig->MQTTHost))
			{
				pConfig->MQTTHost[0] = '\0';
				InvalidValue (Messenger, nLine, "mqtt_host", Value);
			}
		}
		else if (CoreStrEqualN (Key.p, Key.n, "mqtt_port"))
		{
			if (ParseUnsigned (Value, 1, 65535, &nValue))
			{
				pConfig->nMQTTPort = nValue;
			}
			else
			{
				pConfig->nMQTTPort = CONFIG_DEFAULT_PORT;
				InvalidValue (Messenger, nLine, "mqtt_port", Value);
			}
		}
		else if (CoreStrEqualN (Key.p, Key.n, "mqtt_user"))
		{
			pConfig->bHasUser = CopyString (Value, CONFIG_USER_MAX, IsCredentialChar,
							pConfig->MQTTUser);
			if (!pConfig->bHasUser)
			{
				pConfig->MQTTUser[0] = '\0';
				Messenger.Report (nLine, "invalid value for 'mqtt_user' - ignored",
						  NoDetail, "");
			}
		}
		else if (CoreStrEqualN (Key.p, Key.n, "mqtt_password"))
		{
			*pPasswordSeen = true;
			pConfig->bHasPassword = CopyString (Value, CONFIG_PASSWORD_MAX, IsCredentialChar,
							    pConfig->MQTTPassword);
			if (!pConfig->bHasPassword)
			{
				// Never echo the value: it is a secret.
				pConfig->MQTTPassword[0] = '\0';
				Messenger.Report (nLine, "invalid value for 'mqtt_password' - ignored",
						  NoDetail, "");
			}
		}
		else if (CoreStrEqualN (Key.p, Key.n, "device_id"))
		{
			if (!CopyString (Value, CONFIG_DEVICE_ID_MAX, IsDeviceIDChar, pConfig->DeviceID))
			{
				CopyCString (CONFIG_DEFAULT_DEVICE_ID, pConfig->DeviceID);
				InvalidValue (Messenger, nLine, "device_id", Value);
			}
		}
		else if (CoreStrEqualN (Key.p, Key.n, "topic_base"))
		{
			if (   Value.n == 0 || Value.p[Value.n - 1] == '/'
			    || !CopyString (Value, CONFIG_TOPIC_BASE_MAX, IsTopicChar, pConfig->TopicBase))
			{
				CopyCString (CONFIG_DEFAULT_TOPIC_BASE, pConfig->TopicBase);
				InvalidValue (Messenger, nLine, "topic_base", Value);
			}
		}
		else if (CoreStrEqualN (Key.p, Key.n, "ir_frame_gap_ms"))
		{
			if (ParseUnsigned (Value, CONFIG_MIN_GAP_MS, CONFIG_MAX_GAP_MS, &nValue))
			{
				pConfig->nIRFrameGapMs = nValue;
			}
			else
			{
				pConfig->nIRFrameGapMs = CONFIG_DEFAULT_GAP_MS;
				InvalidValue (Messenger, nLine, "ir_frame_gap_ms", Value);
			}
		}
		else
		{
			Messenger.Report (nLine, "unknown key '", Key, "' ignored");
		}
	}
}

void ConfigSetDefaults (TConfig *pConfig)
{
	pConfig->MQTTHost[0] = '\0';
	pConfig->nMQTTPort = CONFIG_DEFAULT_PORT;
	pConfig->bHasUser = false;
	pConfig->MQTTUser[0] = '\0';
	pConfig->bHasPassword = false;
	pConfig->MQTTPassword[0] = '\0';
	CopyCString (CONFIG_DEFAULT_DEVICE_ID, pConfig->DeviceID);
	CopyCString (CONFIG_DEFAULT_TOPIC_BASE, pConfig->TopicBase);
	pConfig->nIRFrameGapMs = CONFIG_DEFAULT_GAP_MS;
}

bool ConfigParse (const char *pText, size_t nLength, TConfig *pConfig,
		  TConfigMessageHandler *pHandler, void *pParam)
{
	CMessenger Messenger (pHandler, pParam);
	bool bPasswordSeen = false;

	ConfigSetDefaults (pConfig);

	size_t nPos = 0;
	unsigned nLine = 0;
	while (nPos < nLength)
	{
		nLine++;

		TSlice Line = { pText + nPos, 0 };
		while (nPos < nLength && pText[nPos] != '\n')
		{
			nPos++;
			Line.n++;
		}
		nPos++;		// skip '\n'

		Line = Trim (Line);
		if (Line.n == 0 || Line.p[0] == '#')
		{
			continue;
		}

		size_t nEquals = 0;
		while (nEquals < Line.n && Line.p[nEquals] != '=')
		{
			nEquals++;
		}

		if (nEquals == Line.n)
		{
			Messenger.Report (nLine, "missing '=' - line ignored", NoDetail, "");
			continue;
		}

		TSlice Key = Trim ({ Line.p, nEquals });
		TSlice Value = Trim ({ Line.p + nEquals + 1, Line.n - nEquals - 1 });

		ApplyKey (pConfig, Key, Value, nLine, Messenger, &bPasswordSeen);
	}

	if (bPasswordSeen && !pConfig->bHasUser)
	{
		pConfig->bHasPassword = false;
		pConfig->MQTTPassword[0] = '\0';
		Messenger.Send ("'mqtt_password' is ignored because 'mqtt_user' is not set");
	}

	if (pConfig->MQTTHost[0] == '\0')
	{
		Messenger.Send ("missing required key 'mqtt_host'");
		return false;
	}

	return true;
}

void ConfigDump (const TConfig &Config, char *pBuffer, size_t nSize)
{
	CTextWriter Writer (pBuffer, nSize);

	Writer.Append ("mqtt_host=");
	Writer.Append (Config.MQTTHost);
	Writer.Append (" mqtt_port=");
	Writer.AppendUnsigned (Config.nMQTTPort);

	if (Config.bHasUser)
	{
		Writer.Append (" mqtt_user=");
		Writer.Append (Config.MQTTUser);
	}

	if (Config.bHasPassword)
	{
		Writer.Append (" mqtt_password=***");
	}

	Writer.Append (" device_id=");
	Writer.Append (Config.DeviceID);
	Writer.Append (" topic_base=");
	Writer.Append (Config.TopicBase);
	Writer.Append (" ir_frame_gap_ms=");
	Writer.AppendUnsigned (Config.nIRFrameGapMs);
}
