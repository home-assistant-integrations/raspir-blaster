/*
 *  logtee.h
 *  raspir-blaster
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

#ifndef _logtee_h
#define _logtee_h

#include "logring.h"
#include <circle/device.h>
#include <circle/interrupt.h>
#include <circle/spinlock.h>
#include <circle/types.h>
#include <circle/usb/gadget/usbcdcgadget.h>

// pid.codes (https://pid.codes): 0x1209 is the shared open-source vendor ID.
// 0x0001 is a pid.codes test product ID - request an own one before release.
#define USB_LOG_VENDOR_ID	0x1209
#define USB_LOG_PRODUCT_ID	0x0001

/// USB CDC ACM gadget that reports DTR (a terminal opened the port) and
/// names itself "raspir-blaster (<device_id>)".
class CLogUSBGadget : public CUSBCDCGadget
{
public:
	CLogUSBGadget (CInterruptSystem *pInterruptSystem, const char *pDeviceID);

	boolean IsDTRSet (void) const	{ return m_bDTR; }
	void ClearDTR (void)		{ m_bDTR = FALSE; }

protected:
	const void *GetDescriptor (u16 wValue, u16 wIndex, size_t *pLength) override;

	// Called on EP0 in interrupt context: only records the DTR state.
	int OnClassOrVendorRequest (const TSetupData *pSetupData, u8 *pData) override;

private:
	volatile boolean m_bDTR;
	char m_Product[64];
};

/// Logger target: writes every log byte to the UART unchanged and keeps it
/// in a CLogRing; Process() sends the ring to the USB serial port while a
/// terminal has it open (see openspec/specs/connectivity, "Diagnostic log on
/// serial"). Write() may be called from interrupt context and never touches
/// USB, so logging never waits for the USB host.
class CLogTee : public CDevice
{
public:
	CLogTee (CDevice *pUART);

	int Write (const void *pBuffer, size_t nCount) override;

	/// Creates and starts the USB gadget (only on boards with USB device mode).
	boolean StartUSB (CInterruptSystem *pInterruptSystem, const char *pDeviceID);

	/// Called from the main task: plug-and-play, DTR, sending (bounded).
	void Process (void);

private:
	static void SerialRemovedHandler (CDevice *pDevice, void *pContext);

private:
	CDevice		*m_pUART;
	CLogRing	m_Ring;
	CSpinLock	m_SpinLock;	// m_Ring; the logger may run at IRQ level

	CLogUSBGadget	*m_pGadget;
	CDevice		*m_pUSBSerial;	// "utty1" while the host has it configured
	boolean		m_bReading;	// DTR was set at the last Process()
};

#endif
