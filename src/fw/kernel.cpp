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
#define WIFI_MARKER_FILE	DRIVE "/" WIFI_MARKER_FILE_NAME

#define CONFIG_FILE_MAX		2048		// bytes
#define WATCHDOG_TIMEOUT_S	10
#define MAIN_LOOP_SLEEP_MS	5
#define RX_UNMUTE_GUARD_MS	5		// after the last mark of a transmission
#define LED_FLASH_MS		50
#define LED_DEGRADED_TOGGLE_MS	1000		// slow blink
#define LED_FATAL_TOGGLE_MS	100		// fast blink
#define LINK_POLL_MS		100		// Ethernet link check interval

static const char FromKernel[] = "kernel";

CKernel::CKernel (void)
:	m_Timer (&m_Interrupt),
	m_Logger (m_Options.GetLogLevel (), &m_Timer),
	m_EMMC (&m_Interrupt, &m_Timer, &m_ActLED),
	m_WLAN (FIRMWARE_PATH),
	m_WPASupplicant (WPA_CONFIG_FILE),
	m_GPIOManager (&m_Interrupt),
#if RASPPI == 3
	m_USBHCI (&m_Interrupt, &m_Timer),
#endif
	m_pNet (0),
	m_pMQTT (0),
	m_Receiver (&m_GPIOManager),
	m_bWiFiConfig (FALSE),
	m_bWiFiMarker (FALSE),
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

	m_NetSelection = NetSelect (BoardHasEthernet (), m_bWiFiConfig, m_bWiFiMarker);

	// Delete the marker before any network bring-up, so that every later
	// boot (also one from the fatal state) checks the Ethernet link again.
	if (m_NetSelection.bDeleteMarker && f_unlink (WIFI_MARKER_FILE) != FR_OK)
	{
		m_Logger.Write (FromKernel, LogWarning, "Cannot delete %s", WIFI_MARKER_FILE);
	}

	if (m_NetSelection.Choice == NetChoiceNone)
	{
		m_Logger.Write (FromKernel, LogError, "Wi-Fi configuration missing: %s",
				WPA_CONFIG_FILE);
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

	// Whether they are needed depends on the board (see InitializeNetwork()).
	FILINFO Info;
	m_bWiFiConfig = f_stat (WPA_CONFIG_FILE, &Info) == FR_OK;
	m_bWiFiMarker = f_stat (WIFI_MARKER_FILE, &Info) == FR_OK;

	return TRUE;
}

void CKernel::ConfigMessageHandler (void *pParam, const char *pMessage)
{
	static_cast<CKernel *> (pParam)->m_Logger.Write ("config", LogWarning, "%s", pMessage);
}

// Chooses and brings up the one network interface of this boot, see
// openspec/specs/connectivity ("Network interface selection").
boolean CKernel::InitializeNetwork (void)
{
	switch (m_NetSelection.Choice)
	{
	case NetChoiceWiFi:
		m_Logger.Write (FromKernel, LogNotice, "Network: Wi-Fi (%s)",
				m_NetSelection.Reason == NetReasonWiFiMarker
					? "after Ethernet check" : "board has no Ethernet");
		return InitializeWiFi ();

	case NetChoiceEthernet:
		m_Logger.Write (FromKernel, LogNotice, "Network: Ethernet (no %s)",
				WPA_CONFIG_FILE);
		return InitializeEthernet (FALSE);

	case NetChoiceProbe:
		return InitializeEthernet (TRUE);

	case NetChoiceNone:
		break;
	}

	return FALSE;
}

// bCheckLink: wpa_supplicant.conf exists, so fall back to Wi-Fi without link.
boolean CKernel::InitializeEthernet (boolean bCheckLink)
{
#if RASPPI == 3
	// The USB chip can be checked before the network is set up.
	if (!StartUSB ())
	{
		if (!bCheckLink)
		{
			return FALSE;
		}

		m_Logger.Write (FromKernel, LogNotice, "Network: Wi-Fi (no Ethernet)");
		return InitializeWiFi ();
	}

	if (bCheckLink)
	{
		boolean bLinkUp = WaitForEthernetLink ();
		if (NetSelectAfterProbe (bLinkUp, TRUE) == NetProbeWiFi)
		{
			m_Logger.Write (FromKernel, LogNotice, "Network: Wi-Fi (no Ethernet link)");
			return InitializeWiFi ();
		}
	}

	return CreateNetSubSystem (NetDeviceTypeEthernet);
#elif RASPPI == 4
	// The GENET driver lives inside the net subsystem: start it on Ethernet,
	// and reboot into Wi-Fi if there is no link (CNetSubSystem cannot be
	// replaced once initialized).
	if (!CreateNetSubSystem (NetDeviceTypeEthernet))
	{
		return FALSE;
	}

	if (bCheckLink)
	{
		boolean bLinkUp = WaitForEthernetLink ();
		if (NetSelectAfterProbe (bLinkUp, FALSE) == NetProbeMarkAndReboot)
		{
			RebootToWiFi ();	// returns only if the marker cannot be written
		}
	}

	return TRUE;
#else
	(void) bCheckLink;
	m_Logger.Write (FromKernel, LogError, "Board has no Ethernet");
	return FALSE;
#endif
}

// Pi 3 B/B+ (USB LAN9514/LAN7515), Pi 4 B and CM4 (GENET; the PHY is on the
// module, the jack on the carrier board).
boolean CKernel::BoardHasEthernet (void)
{
#if RASPPI == 1
	return FALSE;		// Zero W
#else
	switch (CMachineInfo::Get ()->GetMachineModel ())
	{
	case MachineModel3B:
	case MachineModel3BPlus:
	case MachineModel4B:
	case MachineModelCM4:
		return TRUE;

	default:
		return FALSE;
	}
#endif
}

boolean CKernel::StartUSB (void)
{
#if RASPPI == 3
	if (!m_USBHCI.Initialize ())
	{
		m_Logger.Write (FromKernel, LogError, "Cannot initialize USB");
		return FALSE;
	}

	if (CNetDevice::GetNetDevice (NetDeviceTypeEthernet) == 0)
	{
		m_Logger.Write (FromKernel, LogError, "Ethernet chip not found");
		return FALSE;
	}

	return TRUE;
#else
	return FALSE;
#endif
}

boolean CKernel::CreateNetSubSystem (TNetDeviceType DeviceType)
{
	m_pNet = new CNetSubSystem (0, 0, 0, 0, m_Config.DeviceID, DeviceType);
	if (m_pNet == 0 || !m_pNet->Initialize (FALSE))
	{
		m_Logger.Write (FromKernel, LogError, "Cannot initialize network");
		return FALSE;
	}

	return TRUE;
}

// Polls the PHY of the Ethernet chip for up to ETHERNET_LINK_TIMEOUT_MS. Other
// tasks keep running (MsSleep); the watchdog is not started yet.
boolean CKernel::WaitForEthernetLink (void)
{
	CNetDevice *pDevice = 0;
	u64 nStartMs = NowMs ();

	do
	{
		if (pDevice == 0)
		{
			pDevice = CNetDevice::GetNetDevice (NetDeviceTypeEthernet);
		}

		if (pDevice != 0)
		{
			pDevice->UpdatePHY ();
			if (pDevice->IsLinkUp ())
			{
				// Some devices (e.g. USB CDC Ethernet) do not report a speed.
				TNetDeviceSpeed Speed = pDevice->GetLinkSpeed ();
				if (Speed != NetDeviceSpeedUnknown)
				{
					m_Logger.Write (FromKernel, LogNotice, "Network: Ethernet (link up, %s)",
							CNetDevice::GetSpeedString (Speed));
				}
				else
				{
					m_Logger.Write (FromKernel, LogNotice, "Network: Ethernet (link up)");
				}
				return TRUE;
			}
		}

		m_Scheduler.MsSleep (LINK_POLL_MS);
	}
	while (NowMs () - nStartMs < ETHERNET_LINK_TIMEOUT_MS);

	return FALSE;
}

// Pi 4 / CM4 without Ethernet link: the next boot uses Wi-Fi (see NetSelect()).
void CKernel::RebootToWiFi (void)
{
	FIL File;
	if (f_open (&File, WIFI_MARKER_FILE, FA_WRITE | FA_CREATE_ALWAYS) != FR_OK)
	{
		// Rebooting would only try Ethernet again: stay and wait for a cable.
		m_Logger.Write (FromKernel, LogError, "Cannot create %s, staying on Ethernet",
				WIFI_MARKER_FILE);
		return;
	}
	f_close (&File);
	f_mount (0, DRIVE, 0);

	m_Logger.Write (FromKernel, LogNotice, "No Ethernet link, rebooting to use Wi-Fi");
	m_Scheduler.MsSleep (100);	// let the serial log drain

	m_Watchdog.Restart ();
}

boolean CKernel::InitializeWiFi (void)
{
	if (!m_WLAN.Initialize ())
	{
		m_Logger.Write (FromKernel, LogError, "Cannot initialize Wi-Fi");
		return FALSE;
	}

	if (!CreateNetSubSystem (NetDeviceTypeWLAN))
	{
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
