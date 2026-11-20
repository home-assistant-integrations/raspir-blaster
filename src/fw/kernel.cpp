/*
 *  kernel.cpp
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

#include "kernel.h"
#include "textwriter.h"
#include "version.h"
#include <circle/machineinfo.h>
#include <circle/string.h>

#define DRIVE			"SD:"
#define FIRMWARE_PATH		DRIVE "/firmware/"
#define WPA_CONFIG_FILE		DRIVE "/wpa_supplicant.conf"
#define DEVICE_CONFIG_FILE	DRIVE "/" CONFIG_FILE_NAME

#define CONFIG_FILE_MAX		2048		// bytes
#define WATCHDOG_TIMEOUT_S	10
#define MAIN_LOOP_SLEEP_MS	5
#define RX_UNMUTE_GUARD_MS	5		// after the last mark of a transmission
#define LED_FLASH_MS		50
#define LED_DEGRADED_TOGGLE_MS	1000		// slow blink
#define LED_FATAL_TOGGLE_MS	100		// fast blink

static const char FromKernel[] = "kernel";

CKernel::CKernel (void)
:	m_Timer (&m_Interrupt),
	m_Logger (m_Options.GetLogLevel (), &m_Timer),
	m_EMMC (&m_Interrupt, &m_Timer, &m_ActLED),
	m_WLAN (FIRMWARE_PATH),
	m_WPASupplicant (WPA_CONFIG_FILE),
	m_GPIOManager (&m_Interrupt),
	m_pNet (0),
	m_pMQTT (0),
	m_Receiver (&m_GPIOManager),
	m_State (StateRunning),
	m_bNetworkWasUp (FALSE),
	m_nLostFramesLogged (0),
	m_nLEDOffAtMs (0),
	m_nLEDToggleAtMs (0),
	m_bLEDOn (FALSE)
{
	ConfigSetDefaults (&m_Config);
	m_ActLED.Blink (3);	// show we are alive
}

CKernel::~CKernel (void)
{
}

boolean CKernel::Initialize (void)
{
	// Without the serial log nothing can be diagnosed: fail hard.
	if (   !m_Serial.Initialize (115200)
	    || !m_Logger.Initialize (&m_Serial)
	    || !m_Interrupt.Initialize ()
	    || !m_Timer.Initialize ())
	{
		return FALSE;
	}

	m_Logger.Write (FromKernel, LogNotice, "%s %s (" __DATE__ " " __TIME__ ")",
			RASPIR_FIRMWARE_NAME, RASPIR_FIRMWARE_VERSION);

	const char *pBoard = CMachineInfo::Get ()->GetMachineName ();
	m_Logger.Write (FromKernel, LogNotice, "Board: %s", pBoard);

	CTextWriter Model (m_Model, sizeof m_Model);
	Model.Append (pBoard);
	Model.Append (" (Circle bare-metal)");

	if (!InitializeStorage ())
	{
		m_State = StateFatal;
		return TRUE;
	}

	if (!LoadConfig ())
	{
		m_State = StateDegraded;
		return TRUE;
	}

	if (!InitializeNetwork () || !InitializeIR ())
	{
		m_State = StateFatal;
	}

	return TRUE;
}

boolean CKernel::InitializeStorage (void)
{
	if (!m_EMMC.Initialize ())
	{
		m_Logger.Write (FromKernel, LogError, "Cannot initialize SD card");
		return FALSE;
	}

	if (f_mount (&m_FileSystem, DRIVE, 1) != FR_OK)
	{
		m_Logger.Write (FromKernel, LogError, "Cannot mount drive %s", DRIVE);
		return FALSE;
	}

	m_Logger.Write (FromKernel, LogNotice, "SD card mounted");

	return TRUE;
}

boolean CKernel::LoadConfig (void)
{
	FIL File;
	if (f_open (&File, DEVICE_CONFIG_FILE, FA_READ | FA_OPEN_EXISTING) != FR_OK)
	{
		m_Logger.Write (FromKernel, LogError, "Configuration missing: %s", DEVICE_CONFIG_FILE);
		return FALSE;
	}

	static char Buffer[CONFIG_FILE_MAX];
	UINT nRead = 0;
	boolean bTooLarge = f_size (&File) > sizeof Buffer;
	FRESULT Result = bTooLarge ? FR_OK : f_read (&File, Buffer, sizeof Buffer, &nRead);
	f_close (&File);

	if (bTooLarge || Result != FR_OK)
	{
		m_Logger.Write (FromKernel, LogError, "Cannot read %s%s", DEVICE_CONFIG_FILE,
				bTooLarge ? " (larger than 2 KB)" : "");
		return FALSE;
	}

	boolean bValid = ConfigParse (Buffer, nRead, &m_Config, ConfigMessageHandler, this);

	char Dump[256];
	ConfigDump (m_Config, Dump, sizeof Dump);
	m_Logger.Write (FromKernel, LogNotice, "Config: %s", Dump);

	if (!bValid)
	{
		m_Logger.Write (FromKernel, LogError, "Configuration invalid: MQTT disabled");
		return FALSE;
	}

	FILINFO Info;
	if (f_stat (WPA_CONFIG_FILE, &Info) != FR_OK)
	{
		m_Logger.Write (FromKernel, LogError, "Wi-Fi configuration missing: %s",
				WPA_CONFIG_FILE);
		return FALSE;
	}

	return TRUE;
}

void CKernel::ConfigMessageHandler (void *pParam, const char *pMessage)
{
	static_cast<CKernel *> (pParam)->m_Logger.Write ("config", LogWarning, "%s", pMessage);
}

boolean CKernel::InitializeNetwork (void)
{
	if (!m_WLAN.Initialize ())
	{
		m_Logger.Write (FromKernel, LogError, "Cannot initialize Wi-Fi");
		return FALSE;
	}

	m_pNet = new CNetSubSystem (0, 0, 0, 0, m_Config.DeviceID, NetDeviceTypeWLAN);
	if (m_pNet == 0 || !m_pNet->Initialize (FALSE))
	{
		m_Logger.Write (FromKernel, LogError, "Cannot initialize network");
		return FALSE;
	}

	if (!m_WPASupplicant.Initialize ())
	{
		m_Logger.Write (FromKernel, LogError, "Cannot start WPA supplicant");
		return FALSE;
	}

	return TRUE;
}

boolean CKernel::InitializeIR (void)
{
	if (   !m_GPIOManager.Initialize ()
	    || !m_Receiver.Initialize (m_Config.nIRFrameGapMs)
	    || !m_Transmitter.Initialize ())
	{
		m_Logger.Write (FromKernel, LogError, "Cannot initialize IR hardware");
		return FALSE;
	}

	return TRUE;
}

void CKernel::Run (void)
{
	if (m_State == StateFatal)
	{
		// Let the watchdog reboot the device: a transient driver failure
		// may go away on the next boot.
		m_Logger.Write (FromKernel, LogError, "Fatal error, rebooting in %u s",
				WATCHDOG_TIMEOUT_S);
		m_Watchdog.Start (WATCHDOG_TIMEOUT_S);
		for (;;)
		{
			UpdateLED ();
			m_Scheduler.MsSleep (10);
		}
	}

	m_Watchdog.Start (WATCHDOG_TIMEOUT_S);

	if (m_State == StateDegraded)
	{
		m_Logger.Write (FromKernel, LogError, "Degraded mode: fix the configuration on the SD card");
		for (;;)
		{
			FeedWatchdog ();
			UpdateLED ();
			m_Scheduler.MsSleep (50);
		}
	}

	m_pMQTT = new CIRMqttClient (m_pNet, &m_Config, &m_TxQueue, m_Model);

	for (;;)
	{
#ifdef RASPIR_WATCHDOG_TEST
		// Test build (make WATCHDOG_TEST=1): hang after 30 s; the watchdog
		// must reboot the device 10 s later.
		if (NowMs () > 30000)
		{
			m_Logger.Write (FromKernel, LogWarning, "Watchdog test: hanging main loop");
			for (;;)
			{
				// spin without feeding the watchdog or yielding
			}
		}
#endif
		FeedWatchdog ();
		LogNetworkState ();
		ProcessReceivedFrames ();
		ProcessTransmitQueue ();
		UpdateLED ();

		m_Scheduler.MsSleep (MAIN_LOOP_SLEEP_MS);
	}
}

void CKernel::FeedWatchdog (void)
{
	// Restarting the watchdog reloads its counter.
	m_Watchdog.Start (WATCHDOG_TIMEOUT_S);
}

void CKernel::ProcessReceivedFrames (void)
{
	TIRCaptureResult Result;
	while (m_Receiver.Poll (&m_Frame, &Result))
	{
		switch (Result)
		{
		case IRCaptureFrame:
			if (m_pMQTT->IsOnline ())
			{
				m_pMQTT->PublishFrame (m_Frame);
				FlashLED ();
			}
			else
			{
				m_Logger.Write (FromKernel, LogDebug, "Frame discarded: MQTT offline");
			}
			break;

		case IRCaptureOverflow:
			m_Logger.Write (FromKernel, LogWarning,
					"Received frame too long (> %u timings or > 2 s), discarded",
					IR_MAX_TIMINGS);
			break;

		case IRCaptureNoise:
			break;
		}
	}

	unsigned nLost = m_Receiver.GetLostFrames ();
	if (nLost != m_nLostFramesLogged)
	{
		m_Logger.Write (FromKernel, LogWarning, "%u received frame(s) lost (main loop busy)",
				nLost - m_nLostFramesLogged);
		m_nLostFramesLogged = nLost;
	}
}

void CKernel::ProcessTransmitQueue (void)
{
	while (m_TxQueue.Pop (&m_Frame))
	{
		m_Receiver.Mute ();

		boolean bOK = m_Transmitter.Send (m_Frame);
		FlashLED ();

		// Let reflections and the receiver's AGC settle before listening again.
		m_Scheduler.MsSleep (RX_UNMUTE_GUARD_MS);
		m_Receiver.Unmute ();

		if (bOK)
		{
			m_Logger.Write (FromKernel, LogDebug, "Transmitted %u timings at %u Hz",
					m_Frame.nCount, m_Frame.nFreqHz);
		}
		else
		{
			m_Logger.Write (FromKernel, LogError, "Transmission failed");
		}

		FeedWatchdog ();
	}
}

void CKernel::LogNetworkState (void)
{
	boolean bUp = m_pNet->IsRunning ();
	if (bUp == m_bNetworkWasUp)
	{
		return;
	}
	m_bNetworkWasUp = bUp;

	if (bUp)
	{
		CString IPString;
		m_pNet->GetConfig ()->GetIPAddress ()->Format (&IPString);
		m_Logger.Write (FromKernel, LogNotice, "Network up, IP address %s",
				(const char *) IPString);
	}
	else
	{
		m_Logger.Write (FromKernel, LogWarning, "Network down");
	}
}

u64 CKernel::NowMs (void)
{
	return CTimer::GetClockTicks64 () / 1000;
}

void CKernel::FlashLED (void)
{
	m_nLEDOffAtMs = NowMs () + LED_FLASH_MS;
	m_ActLED.On ();
	m_bLEDOn = TRUE;
}

void CKernel::UpdateLED (void)
{
	u64 nNow = NowMs ();

	if (m_State == StateRunning)
	{
		if (m_bLEDOn && nNow >= m_nLEDOffAtMs)
		{
			m_ActLED.Off ();
			m_bLEDOn = FALSE;
		}
		return;
	}

	if (nNow >= m_nLEDToggleAtMs)
	{
		m_bLEDOn = !m_bLEDOn;
		if (m_bLEDOn)
		{
			m_ActLED.On ();
		}
		else
		{
			m_ActLED.Off ();
		}

		m_nLEDToggleAtMs = nNow + (m_State == StateDegraded ? LED_DEGRADED_TOGGLE_MS
								    : LED_FATAL_TOGGLE_MS);
	}
}
