/*
 *  manifest.h
 *  raspir-blaster core
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

#ifndef _raspir_manifest_h
#define _raspir_manifest_h

#include <stddef.h>
#include <stdint.h>

// raspir-blaster-manifest.txt in release zips (openspec/specs/firmware-build,
// "SD card image directory"):
//   version=<firmware version>
//   build=sdcard | cm4-emmc
//   sha256.<image>=<64 lower-case hex digits>   (one line per kernel image)

#define MANIFEST_FILE_NAME	"raspir-blaster-manifest.txt"
#define MANIFEST_MAX_IMAGES	4
#define MANIFEST_VERSION_MAX	31
#define MANIFEST_BUILD_MAX	15
#define MANIFEST_IMAGE_MAX	31

struct TManifestImage
{
	char Name[MANIFEST_IMAGE_MAX + 1];
	char SHA256[65];		// lower-case hex
};

struct TManifest
{
	char		Version[MANIFEST_VERSION_MAX + 1];
	char		Build[MANIFEST_BUILD_MAX + 1];
	unsigned	nImages;
	TManifestImage	Images[MANIFEST_MAX_IMAGES];
};

/// Parses the manifest. Blank lines and lines starting with '#' are ignored,
/// "\r\n" is accepted. Fails on malformed lines, a missing version or build,
/// a duplicate key, more than MANIFEST_MAX_IMAGES images, or an invalid hash.
bool ManifestParse (const char *pText, size_t nLength, TManifest *pManifest);

/// \return the hex SHA-256 of the image, or 0 if the manifest has none for it
const char *ManifestFindSHA256 (const TManifest &Manifest, const char *pImage);

enum TVersionOrder
{
	VersionNewer,		// the candidate is newer than the installed version
	VersionSame,
	VersionOlder,
	VersionNotComparable	// at least one is not of the form yyyy.mm.rm (e.g. "dev")
};

/// Compares release versions yyyy.mm.rm numerically, field by field.
TVersionOrder VersionCompare (const char *pInstalled, const char *pCandidate);

#endif
