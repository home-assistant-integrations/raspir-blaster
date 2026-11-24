/*
 *  logtee.cpp
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

#include "logtee.h"
#include "textwriter.h"
#include <circle/devicenameservice.h>
#include <circle/string.h>
#include <circle/usb/usb.h>

#define USB_SERIAL_DEVICE	"utty1"
#define CDC_SET_CONTROL_LINE_STATE	0x22
#define CDC_LINE_STATE_DTR	0x01
#define SEND_CHUNK		512	// bytes copied out of the ring per write
#define SEND_MAX_CHUNKS		8	// per Process() call: at most 4 KB

#define STRING_MANUFACTURER	1	// string indices of CUSBCDCGadget
#define STRING_PRODUCT		2

static const char Manufacturer[] = "raspir-blaster";

// --- CLogUSBGadget -------------------------------------------------------------

CLogUSBGadget::CLogUSBGadget (CInterruptSystem *pInterruptSystem, const char *pDeviceID)
:	CUSBCDCGadget (pInterruptSystem, USB_LOG_VENDOR_ID, USB_LOG_PRODUCT_ID),
	m_bDTR (FALSE)
{
	CTextWriter Product (m_Product, sizeof m_Product);
	Product.Append ("raspir-blaster (");
	Product.Append (pDeviceID);
	Product.Append (")");
}

const void *CLogUSBGadget::GetDescriptor (u16 wValue, u16 wIndex, size_t *pLength)
{
	if ((wValue >> 8) == DESCRIPTOR_STRING)
	{
		switch (wValue & 0xFF)
		{
		case STRING_MANUFACTURER:
			return ToStringDescriptor (Manufacturer, pLength);

		case STRING_PRODUCT:
			return ToStringDescriptor (m_Product, pLength);

		default:
			break;
		}
	}

	return CUSBCDCGadget::GetDescriptor (wValue, wIndex, pLength);
}

int CLogUSBGadget::OnClassOrVendorRequest (const TSetupData *pSetupData, u8 *pData)
{
	if (   pSetupData->bmRequestType == (REQUEST_OUT | REQUEST_CLASS | REQUEST_TO_INTERFACE)
	    && pSetupData->bRequest == CDC_SET_CONTROL_LINE_STATE)
	{
		m_bDTR = pSetupData->wValue & CDC_LINE_STATE_DTR ? TRUE : FALSE;
	}

	return CUSBCDCGadget::OnClassOrVendorRequest (pSetupData, pData);
}

// --- CLogTee ----------------------------------------------------------------------

CLogTee::CLogTee (CDevice *pUART)
:	m_pUART (pUART),
	m_SpinLock (IRQ_LEVEL),
	m_pGadget (0),
	m_pUSBSerial (0),
	m_bReading (FALSE)
{
#ifdef RASPIR_DIAG_SDLOG
	m_nDiag = 0;
#endif
}

int CLogTee::Write (const void *pBuffer, size_t nCount)
{
	m_pUART->Write (pBuffer, nCount);

	m_SpinLock.Acquire ();
	m_Ring.Append (static_cast<const char *> (pBuffer), nCount);
#ifdef RASPIR_DIAG_SDLOG
	for (size_t i = 0; i < nCount; i++)
	{
		if (m_nDiag < sizeof m_Diag)
		{
			m_Diag[m_nDiag++] = static_cast<const char *> (pBuffer)[i];
		}
	}
#endif
	m_SpinLock.Release ();

	return (int) nCount;
}

boolean CLogTee::StartUSB (CInterruptSystem *pInterruptSystem, const char *pDeviceID)
{
	CLogUSBGadget *pGadget = new CLogUSBGadget (pInterruptSystem, pDeviceID);
	if (pGadget == 0 || !pGadget->Initialize ())
	{
		return FALSE;	// a half-initialized gadget is not deleted (owns the controller)
	}

	m_pGadget = pGadget;

	return TRUE;
}

void CLogTee::Process (void)
{
	if (m_pGadget == 0)
	{
		return;
	}

	m_pGadget->UpdatePlugAndPlay ();

	if (m_pUSBSerial == 0)
	{
		m_pUSBSerial = CDeviceNameService::Get ()->GetDevice (USB_SERIAL_DEVICE, FALSE);
		if (m_pUSBSerial == 0)
		{
			return;
		}

		m_pUSBSerial->RegisterRemovedHandler (SerialRemovedHandler, this);
	}

	// A terminal opened the port: replay; closed it: stop sending.
	boolean bDTR = m_pGadget->IsDTRSet ();
	if (bDTR != m_bReading)
	{
		m_SpinLock.Acquire ();
		if (bDTR)
		{
			m_Ring.StartReader ();
		}
		else
		{
			m_Ring.StopReader ();
		}
		m_SpinLock.Release ();

		m_bReading = bDTR;
	}

	if (!m_bReading)
	{
		return;
	}

	char Chunk[SEND_CHUNK];

	m_SpinLock.Acquire ();
	u64 nLost = m_Ring.TakeLost ();
	m_SpinLock.Release ();

	if (nLost > 0)
	{
		CString Marker;
		Marker.Format ("\r\n[log: %llu bytes lost]\r\n", (unsigned long long) nLost);
		m_pUSBSerial->Write ((const char *) Marker, Marker.GetLength ());
	}

	for (unsigned i = 0; i < SEND_MAX_CHUNKS; i++)
	{
		// Copy out under the lock, write without it: the USB driver may log.
		m_SpinLock.Acquire ();
		const char *pData;
		size_t nLength = m_Ring.Peek (&pData);
		if (nLength > sizeof Chunk)
		{
			nLength = sizeof Chunk;
		}
		for (size_t j = 0; j < nLength; j++)
		{
			Chunk[j] = pData[j];
		}
		m_SpinLock.Release ();

		if (nLength == 0)
		{
			break;
		}

		int nWritten = m_pUSBSerial->Write (Chunk, nLength);
		if (nWritten <= 0)
		{
			break;		// queue full or error: try again next time
		}

		m_SpinLock.Acquire ();
		m_Ring.Consume ((size_t) nWritten);
		m_SpinLock.Release ();

		if ((size_t) nWritten < nLength)
		{
			break;		// queue full
		}
	}
}

#ifdef RASPIR_DIAG_SDLOG
size_t CLogTee::TakeDiag (char *pBuffer, size_t nMax)
{
	m_SpinLock.Acquire ();
	size_t nLength = m_nDiag < nMax ? m_nDiag : nMax;
	for (size_t i = 0; i < nLength; i++)
	{
		pBuffer[i] = m_Diag[i];
	}
	for (size_t i = nLength; i < m_nDiag; i++)
	{
		m_Diag[i - nLength] = m_Diag[i];
	}
	m_nDiag -= nLength;
	m_SpinLock.Release ();

	return nLength;
}
#endif

// Host detached or reconfigured: utty1 is gone. Called from UpdatePlugAndPlay().
void CLogTee::SerialRemovedHandler (CDevice *pDevice, void *pContext)
{
	(void) pDevice;
	CLogTee *pThis = static_cast<CLogTee *> (pContext);

	pThis->m_SpinLock.Acquire ();
	pThis->m_Ring.StopReader ();
	pThis->m_SpinLock.Release ();

	pThis->m_pUSBSerial = 0;
	pThis->m_bReading = FALSE;
	pThis->m_pGadget->ClearDTR ();
}
