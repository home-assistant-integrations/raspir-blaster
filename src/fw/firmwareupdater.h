/*
 *  firmwareupdater.h
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

#ifndef _raspir_firmwareupdater_h
#define _raspir_firmwareupdater_h

#include "manifest.h"
#include <circle/bcmrandom.h>
#include <circle/types.h>

#define FW_UPDATE_MAX_IMAGE_SIZE	(4u * 1024u * 1024u)	// 4 MB
#define FW_UPDATE_PENDING_TIMEOUT_MS	(10u * 60u * 1000u)	// 10 minutes
#define FW_UPDATE_TOKEN_LEN		32			// 128-bit, hex
#define FW_UPDATE_REBOOT_DELAY_MS	2000u			// lets the response go out

enum TFirmwareUpdateOutcome
{
	FirmwareUpdateInstalled,		// installed immediately, device reboots
	FirmwareUpdateConfirmRequired,		// needs POST /api/update/confirm
	FirmwareUpdateError,
};

/// Result of Validate(). ErrorMessage, InstalledVersion, UploadedVersion and
/// Token are valid only for the Outcome that sets them (see Validate()).
struct TFirmwareValidateResult
{
	TFirmwareUpdateOutcome	Outcome;
	char			ErrorMessage[96];	// Outcome == FirmwareUpdateError
	char			InstalledVersion[MANIFEST_VERSION_MAX + 1];
	char			UploadedVersion[MANIFEST_VERSION_MAX + 1];
	TVersionOrder		Reason;			// Outcome == FirmwareUpdateConfirmRequired
	char			Token[FW_UPDATE_TOKEN_LEN + 1];
};

/// Validates an uploaded release zip, extracts and installs the kernel
/// image for this board's build target, or keeps it pending for
/// POST /api/update/confirm. src/fw only: uses FatFs and the hardware RNG.
class CFirmwareUpdater
{
public:
	CFirmwareUpdater (void);
	~CFirmwareUpdater (void);

	/// Checks the zip (manifest, build match, extraction, checksum,
	/// version) and, if newer than the running firmware, installs it at
	/// once (Outcome FirmwareUpdateInstalled; see IsRebootPending()).
	/// Otherwise keeps the extracted image pending (Outcome
	/// FirmwareUpdateConfirmRequired, with a one-time Token) or reports
	/// why it was refused (Outcome FirmwareUpdateError).
	/// Refuses a second call while one is already in progress.
	TFirmwareValidateResult Validate (const uint8_t *pZip, size_t nZipLen, u64 nNowMs);

	/// Installs the image kept pending from Validate() if pToken matches
	/// and has not expired. \return 0 on success, else an error message.
	const char *Confirm (const char *pToken, u64 nNowMs);

	/// Discards the image kept pending from Validate(), if any.
	/// \return true if there was a pending update to cancel.
	boolean Cancel (void);

	/// True while no Validate()/Confirm() may run concurrently with another.
	boolean IsBusy (void) const	{ return m_bBusy; }

	/// True once a successful install has set a reboot time; the main
	/// loop checks this and reboots via CBcmWatchdog::Restart() once
	/// NowMs() has passed GetRebootAtMs().
	boolean IsRebootPending (void) const	{ return m_bRebootPending; }
	u64 GetRebootAtMs (void) const		{ return m_nRebootAtMs; }

	/// Arms the reboot path for POST /api/restart (no pending image).
	void ScheduleRestart (u64 nNowMs);

private:
	const char *Install (const uint8_t *pImage, size_t nImageLen);
	void DiscardPending (void);
	void GenerateToken (char Token[FW_UPDATE_TOKEN_LEN + 1]);

	static const char *GetImageName (void);
	static const char *GetBuildName (void);

private:
	boolean		m_bBusy;

	uint8_t		*m_pPendingImage;
	size_t		 m_nPendingImageLen;
	char		 m_PendingToken[FW_UPDATE_TOKEN_LEN + 1];
	u64		 m_nPendingExpiryMs;

	boolean		 m_bRebootPending;
	u64		 m_nRebootAtMs;

	CBcmRandomNumberGenerator m_RNG;
};

#endif
