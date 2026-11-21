/*
 *  firmwareupdater.cpp
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

#include "firmwareupdater.h"
#include "sha256.h"
#include "textwriter.h"
#include "version.h"
#include "zipreader.h"
#include <circle/logger.h>
#include <circle/sched/scheduler.h>
#include <circle/util.h>
#include <fatfs/ff.h>

#define DRIVE			"SD:"
#define WRITE_CHUNK_SIZE	(64u * 1024u)
#define MANIFEST_BUF_SIZE	4096u

static const char FromUpdater[] = "update";
static const char HexDigits[]   = "0123456789abcdef";

// strncpy does not NUL-terminate when pSrc is as long as or longer than
// nDestSize; this always leaves pDest NUL-terminated.
static void CopyTrunc (char *pDest, size_t nDestSize, const char *pSrc)
{
	strncpy (pDest, pSrc, nDestSize - 1);
	pDest[nDestSize - 1] = '\0';
}

CFirmwareUpdater::CFirmwareUpdater (void)
:	m_bBusy (FALSE),
	m_pPendingImage (0),
	m_nPendingImageLen (0),
	m_nPendingExpiryMs (0),
	m_bRebootPending (FALSE),
	m_nRebootAtMs (0)
{
	m_PendingToken[0] = '\0';
}

CFirmwareUpdater::~CFirmwareUpdater (void)
{
	DiscardPending ();
}

const char *CFirmwareUpdater::GetImageName (void)
{
#if RASPPI == 1
	return "kernel.img";
#elif RASPPI == 3
	return "kernel8.img";
#else
	return "kernel8-rpi4.img";
#endif
}

const char *CFirmwareUpdater::GetBuildName (void)
{
#ifdef USE_EMBEDDED_MMC_CM
	return "cm4-emmc";
#else
	return "sdcard";
#endif
}

void CFirmwareUpdater::DiscardPending (void)
{
	delete [] m_pPendingImage;
	m_pPendingImage = 0;
	m_nPendingImageLen = 0;
	m_PendingToken[0] = '\0';
	m_nPendingExpiryMs = 0;
}

void CFirmwareUpdater::GenerateToken (char Token[FW_UPDATE_TOKEN_LEN + 1])
{
	for (unsigned nWord = 0; nWord < 4; nWord++)
	{
		u32 nValue = m_RNG.GetNumber ();
		for (unsigned nNibble = 0; nNibble < 8; nNibble++)
		{
			Token[nWord * 8 + nNibble] = HexDigits[(nValue >> (28 - nNibble * 4)) & 0xF];
		}
	}
	Token[FW_UPDATE_TOKEN_LEN] = '\0';
}

TFirmwareValidateResult CFirmwareUpdater::Validate (const uint8_t *pZip, size_t nZipLen, u64 nNowMs)
{
	TFirmwareValidateResult Result;
	Result.Outcome = FirmwareUpdateError;
	Result.ErrorMessage[0] = '\0';
	Result.InstalledVersion[0] = '\0';
	Result.UploadedVersion[0] = '\0';
	Result.Reason = VersionNotComparable;
	Result.Token[0] = '\0';

	if (m_bBusy)
	{
		CopyTrunc (Result.ErrorMessage, sizeof Result.ErrorMessage,
		          "update already in progress");
		return Result;
	}

	m_bBusy = TRUE;

	static uint8_t s_ManifestBuf[MANIFEST_BUF_SIZE];
	size_t nManifestLen = 0;
	TZipResult ZipResult = ZipExtract (pZip, nZipLen, MANIFEST_FILE_NAME,
	                                   s_ManifestBuf, sizeof s_ManifestBuf, &nManifestLen);
	if (ZipResult != ZipOK)
	{
		CopyTrunc (Result.ErrorMessage, sizeof Result.ErrorMessage,
		          "zip has no readable " MANIFEST_FILE_NAME);
		m_bBusy = FALSE;
		return Result;
	}

	TManifest Manifest;
	if (!ManifestParse ((const char *) s_ManifestBuf, nManifestLen, &Manifest))
	{
		CopyTrunc (Result.ErrorMessage, sizeof Result.ErrorMessage, "manifest is malformed");
		m_bBusy = FALSE;
		return Result;
	}

	if (strcmp (Manifest.Build, GetBuildName ()) != 0)
	{
		CTextWriter W (Result.ErrorMessage, sizeof Result.ErrorMessage);
		W.Append ("zip is for build '");
		W.Append (Manifest.Build);
		W.Append ("', this device is '");
		W.Append (GetBuildName ());
		W.Append ("'");
		m_bBusy = FALSE;
		return Result;
	}

	uint8_t *pImage = new uint8_t[FW_UPDATE_MAX_IMAGE_SIZE];
	if (pImage == 0)
	{
		CopyTrunc (Result.ErrorMessage, sizeof Result.ErrorMessage, "out of memory");
		m_bBusy = FALSE;
		return Result;
	}

	size_t nImageLen = 0;
	ZipResult = ZipExtract (pZip, nZipLen, GetImageName (),
	                        pImage, FW_UPDATE_MAX_IMAGE_SIZE, &nImageLen);
	if (ZipResult != ZipOK)
	{
		delete [] pImage;
		CTextWriter W (Result.ErrorMessage, sizeof Result.ErrorMessage);
		W.Append ("zip does not contain a readable '");
		W.Append (GetImageName ());
		W.Append ("' for this board");
		m_bBusy = FALSE;
		return Result;
	}

	const char *pExpectedHash = ManifestFindSHA256 (Manifest, GetImageName ());
	if (pExpectedHash == 0)
	{
		delete [] pImage;
		CTextWriter W (Result.ErrorMessage, sizeof Result.ErrorMessage);
		W.Append ("manifest has no checksum for '");
		W.Append (GetImageName ());
		W.Append ("'");
		m_bBusy = FALSE;
		return Result;
	}

	CSHA256 Hash;
	Hash.Update (pImage, nImageLen);
	char HexHash[SHA256_HEX_SIZE];
	Hash.FinalHex (HexHash);

	if (strcmp (HexHash, pExpectedHash) != 0)
	{
		delete [] pImage;
		CopyTrunc (Result.ErrorMessage, sizeof Result.ErrorMessage,
		          "checksum mismatch for the extracted image");
		m_bBusy = FALSE;
		return Result;
	}

	TVersionOrder Order = VersionCompare (RASPIR_FIRMWARE_VERSION, Manifest.Version);
	CopyTrunc (Result.InstalledVersion, sizeof Result.InstalledVersion, RASPIR_FIRMWARE_VERSION);
	CopyTrunc (Result.UploadedVersion, sizeof Result.UploadedVersion, Manifest.Version);

	if (Order == VersionNewer)
	{
		const char *pInstallError = Install (pImage, nImageLen);
		delete [] pImage;

		if (pInstallError != 0)
		{
			CopyTrunc (Result.ErrorMessage, sizeof Result.ErrorMessage, pInstallError);
			m_bBusy = FALSE;
			return Result;
		}

		Result.Outcome = FirmwareUpdateInstalled;
		m_bBusy = FALSE;
		ScheduleRestart (nNowMs);
		return Result;
	}

	// Same, older or not-comparable version: keep the image pending.
	DiscardPending ();
	m_pPendingImage = pImage;
	m_nPendingImageLen = nImageLen;
	GenerateToken (m_PendingToken);
	m_nPendingExpiryMs = nNowMs + FW_UPDATE_PENDING_TIMEOUT_MS;

	Result.Outcome = FirmwareUpdateConfirmRequired;
	Result.Reason = Order;
	CopyTrunc (Result.Token, sizeof Result.Token, m_PendingToken);

	m_bBusy = FALSE;
	return Result;
}

const char *CFirmwareUpdater::Confirm (const char *pToken, u64 nNowMs)
{
	if (m_bBusy)
	{
		return "update already in progress";
	}

	if (m_pPendingImage == 0)
	{
		return "no update pending";
	}

	if (nNowMs > m_nPendingExpiryMs)
	{
		DiscardPending ();
		return "token expired";
	}

	if (strcmp (pToken, m_PendingToken) != 0)
	{
		return "unknown token";
	}

	m_bBusy = TRUE;
	const char *pError = Install (m_pPendingImage, m_nPendingImageLen);
	DiscardPending ();
	m_bBusy = FALSE;

	if (pError == 0)
	{
		ScheduleRestart (nNowMs);
	}

	return pError;
}

boolean CFirmwareUpdater::Cancel (void)
{
	if (m_bBusy || m_pPendingImage == 0)
	{
		return FALSE;
	}

	DiscardPending ();
	return TRUE;
}

void CFirmwareUpdater::ScheduleRestart (u64 nNowMs)
{
	m_bRebootPending = TRUE;
	m_nRebootAtMs = nNowMs + FW_UPDATE_REBOOT_DELAY_MS;
}

// Writes <image>.new in chunks, reads it back and checks its hash, then
// swaps it in: delete <image>.bak, rename <image> to <image>.bak, rename
// <image>.new to <image>. On any failure after the write, <image> is left
// untouched and <image>.new is removed.
const char *CFirmwareUpdater::Install (const uint8_t *pImage, size_t nImageLen)
{
	const char *pImageName = GetImageName ();

	char NewPath[64], BakPath[64], CurPath[64];
	CTextWriter NewW (NewPath, sizeof NewPath);
	NewW.Append (DRIVE "/"); NewW.Append (pImageName); NewW.Append (".new");
	CTextWriter BakW (BakPath, sizeof BakPath);
	BakW.Append (DRIVE "/"); BakW.Append (pImageName); BakW.Append (".bak");
	CTextWriter CurW (CurPath, sizeof CurPath);
	CurW.Append (DRIVE "/"); CurW.Append (pImageName);

	FIL File;
	if (f_open (&File, NewPath, FA_WRITE | FA_CREATE_ALWAYS) != FR_OK)
	{
		return "cannot create new image file";
	}

	size_t nWritten = 0;
	while (nWritten < nImageLen)
	{
		UINT nChunk = (UINT) ((nImageLen - nWritten) < WRITE_CHUNK_SIZE
		                       ? (nImageLen - nWritten) : WRITE_CHUNK_SIZE);
		UINT nActuallyWritten = 0;
		if (   f_write (&File, pImage + nWritten, nChunk, &nActuallyWritten) != FR_OK
		    || nActuallyWritten != nChunk)
		{
			f_close (&File);
			f_unlink (NewPath);
			return "write error (card full?)";
		}
		nWritten += nActuallyWritten;

		// Yield between chunks so the main task keeps feeding the
		// watchdog during a multi-megabyte write.
		CScheduler::Get ()->Yield ();
	}

	f_sync (&File);
	f_close (&File);

	// Read back and verify.
	if (f_open (&File, NewPath, FA_READ | FA_OPEN_EXISTING) != FR_OK)
	{
		f_unlink (NewPath);
		return "cannot reopen new image file for verification";
	}

	CSHA256 Hash;
	static uint8_t s_ReadBuf[WRITE_CHUNK_SIZE];
	for (;;)
	{
		UINT nRead = 0;
		if (f_read (&File, s_ReadBuf, sizeof s_ReadBuf, &nRead) != FR_OK)
		{
			f_close (&File);
			f_unlink (NewPath);
			return "read-back failed";
		}
		if (nRead == 0)
		{
			break;
		}
		Hash.Update (s_ReadBuf, nRead);
		CScheduler::Get ()->Yield ();
	}
	f_close (&File);

	char HexHash[SHA256_HEX_SIZE];
	Hash.FinalHex (HexHash);

	CSHA256 ExpectedHash;
	ExpectedHash.Update (pImage, nImageLen);
	char ExpectedHexHash[SHA256_HEX_SIZE];
	ExpectedHash.FinalHex (ExpectedHexHash);

	if (strcmp (HexHash, ExpectedHexHash) != 0)
	{
		f_unlink (NewPath);
		return "read-back checksum mismatch";
	}

	f_unlink (BakPath);	// ignore result: may not exist

	if (f_rename (CurPath, BakPath) != FR_OK)
	{
		f_unlink (NewPath);
		return "cannot back up the current image";
	}

	if (f_rename (NewPath, CurPath) != FR_OK)
	{
		// Restore the previous image so the device can still boot.
		f_rename (BakPath, CurPath);
		return "cannot install the new image";
	}

	CLogger::Get ()->Write (FromUpdater, LogNotice, "Installed %s, rebooting in %u ms",
	                       pImageName, FW_UPDATE_REBOOT_DELAY_MS);

	return 0;
}
