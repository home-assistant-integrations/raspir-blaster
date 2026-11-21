/*
 *  kernel.h
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

#ifndef _kernel_h
#define _kernel_h

#include "config.h"
#include "irmessage.h"
#include "irmqttclient.h"
#include "irreceiver.h"
#include "irtransmitter.h"
#include "irtxqueue.h"
#include "logtee.h"
#include "netselect.h"
#include <circle/actled.h>
#include <circle/bcmwatchdog.h>
#include <circle/devicenameservice.h>
#include <circle/exceptionhandler.h>
#include <circle/gpiomanager.h>
#include <circle/interrupt.h>
#include <circle/koptions.h>
#include <circle/logger.h>
#include <circle/net/netsubsystem.h>
#include <circle/sched/scheduler.h>
#include <circle/serial.h>
#include <circle/timer.h>
#include <circle/types.h>
#if RASPPI == 3
#include <circle/usb/usbhcidevice.h>	// Ethernet of the Pi 3 B/B+ is a USB chip
#endif
#include <SDCard/emmc.h>
#include <fatfs/ff.h>
#include <wlan/bcm4343.h>
#include <wlan/hostap/wpa_supplicant/wpasupplicant.h>

class CKernel
{
public:
	CKernel (void);
	~CKernel (void);

	boolean Initialize (void);
	void Run (void);

private:
	enum TState
	{
		StateRunning,
		StateDegraded,		// configuration problem: no MQTT, slow blink, no reboot
		StateFatal		// driver failure: reboot via the watchdog
	};

	boolean InitializeStorage (void);
	boolean LoadConfig (void);
	boolean InitializeNetwork (void);
	boolean InitializeWiFi (void);
	boolean InitializeEthernet (boolean bCheckLink);
	boolean StartUSB (void);
	boolean CreateNetSubSystem (TNetDeviceType DeviceType);
	boolean WaitForEthernetLink (void);
	void RebootToWiFi (void);
	boolean InitializeIR (void);

	void StartUSBLog (void);

	static boolean BoardHasEthernet (void);
	static boolean BoardHasUSBDevice (void);

	void FeedWatchdog (void);
	void ProcessReceivedFrames (void);
	void ProcessTransmitQueue (void);
	void LogNetworkState (void);
	void UpdateLED (void);
	void FlashLED (void);

	static void ConfigMessageHandler (void *pParam, const char *pMessage);
	static u64 NowMs (void);

private:
	// Initialization order matters - do not reorder.
	CActLED			m_ActLED;
	CKernelOptions		m_Options;
	CDeviceNameService	m_DeviceNameService;
	CSerialDevice		m_Serial;
	CLogTee			m_LogTee;	// logger target: UART + USB serial
	CExceptionHandler	m_ExceptionHandler;
	CInterruptSystem	m_Interrupt;
	CTimer			m_Timer;
	CLogger			m_Logger;
	CScheduler		m_Scheduler;
	CBcmWatchdog		m_Watchdog;
	CEMMCDevice		m_EMMC;
	FATFS			m_FileSystem;
	CBcm4343Device		m_WLAN;
	CWPASupplicant		m_WPASupplicant;
	CGPIOManager		m_GPIOManager;
#if RASPPI == 3
	CUSBHCIDevice		m_USBHCI;	// only initialized for Ethernet
#endif

	CNetSubSystem		*m_pNet;	// created after the config (host name)
	CIRMqttClient		*m_pMQTT;
	CIRReceiver		m_Receiver;
	CIRTransmitter		m_Transmitter;
	CIRTxQueue		m_TxQueue;

	TConfig			m_Config;
	boolean			m_bWiFiConfig;	// wpa_supplicant.conf exists
	boolean			m_bWiFiMarker;	// marker file of the previous boot exists
	TNetSelection		m_NetSelection;
	char			m_Model[96];	// "<board name> (Circle bare-metal)"
	TIRFrame		m_Frame;	// scratch frame for the main loop
	TState			m_State;

	boolean			m_bNetworkWasUp;
	unsigned		m_nLostFramesLogged;
	u64			m_nLEDOffAtMs;	// end of an activity flash
	u64			m_nLEDToggleAtMs;
	boolean			m_bLEDOn;
};

#endif
