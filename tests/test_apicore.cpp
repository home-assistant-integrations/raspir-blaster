/*
 *  test_apicore.cpp
 *  raspir-blaster tests
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

// Tests for the helpers of the HTTP API (web-update spec): SHA-256, the
// release manifest and version comparison, form data, JSON.

#include "test.h"
#include "formdata.h"
#include "jsonwriter.h"
#include "manifest.h"
#include "sha256.h"
#include <string>

static std::string Sha256Hex (const std::string &Data, size_t nChunk = 1000000)
{
	CSHA256 Hash;
	for (size_t i = 0; i < Data.size (); i += nChunk)
	{
		Hash.Update (Data.data () + i, Data.size () - i < nChunk ? Data.size () - i : nChunk);
	}
	char Hex[SHA256_HEX_SIZE];
	Hash.FinalHex (Hex);
	return Hex;
}

// --- SHA-256 (FIPS 180-4 test vectors) -------------------------------------------

TEST (Sha256Empty)
{
	CHECK_STR (Sha256Hex ("").c_str (),
		   "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
}

TEST (Sha256Abc)
{
	CHECK_STR (Sha256Hex ("abc").c_str (),
		   "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
}

TEST (Sha256TwoBlocks)
{
	CHECK_STR (Sha256Hex ("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq").c_str (),
		   "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1");
}

TEST (Sha256MillionA)
{
	std::string A (1000000, 'a');
	const char *pExpected = "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0";
	CHECK_STR (Sha256Hex (A).c_str (), pExpected);
	CHECK_STR (Sha256Hex (A, 63).c_str (), pExpected);	// odd chunks, incremental
}

// --- Manifest -------------------------------------------------------------------------

static const char ManifestText[] =
	"# raspir-blaster release manifest\r\n"
	"version=2026.12.0\r\n"
	"build=sdcard\r\n"
	"sha256.kernel.img=e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855\n"
	"sha256.kernel8.img=ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad\n"
	"\n"
	"future_key=ignored\n";

TEST (ManifestParsesAllFields)
{
	TManifest M;
	CHECK (ManifestParse (ManifestText, sizeof ManifestText - 1, &M));
	CHECK_STR (M.Version, "2026.12.0");
	CHECK_STR (M.Build, "sdcard");
	CHECK_EQ (M.nImages, 2u);
	CHECK_STR (ManifestFindSHA256 (M, "kernel8.img"),
		   "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
	CHECK (ManifestFindSHA256 (M, "kernel8-rpi4.img") == 0);
}

static bool Parses (const std::string &Text)
{
	TManifest M;
	return ManifestParse (Text.data (), Text.size (), &M);
}

TEST (ManifestRejectsMalformed)
{
	const std::string Hash (64, 'a');
	CHECK (!Parses ("build=sdcard\n"));				// no version
	CHECK (!Parses ("version=2026.12.0\n"));			// no build
	CHECK (!Parses ("version=1\nversion=2\nbuild=sdcard\n"));	// duplicate
	CHECK (!Parses ("version=1\nbuild=sdcard\nno equals sign\n"));
	CHECK (!Parses ("version=1\nbuild=sdcard\nsha256.k.img=" + std::string (63, 'a') + "\n"));
	CHECK (!Parses ("version=1\nbuild=sdcard\nsha256.k.img=" + std::string (64, 'A') + "\n"));
	CHECK (!Parses ("version=1\nbuild=sdcard\nsha256.k.img=" + Hash + "\nsha256.k.img=" + Hash + "\n"));
	CHECK (!Parses ("version=1\nbuild=sdcard\nsha256.k/x=" + Hash + "\n"));
	CHECK (!Parses ("version=\nbuild=sdcard\n"));
	CHECK (Parses ("version=dev\nbuild=cm4-emmc\nsha256.kernel8-rpi4.img=" + Hash));	// no final LF
}

// --- Version comparison ---------------------------------------------------------------

TEST (VersionCompareOrders)
{
	CHECK_EQ (VersionCompare ("2026.11.2", "2026.12.0"), VersionNewer);
	CHECK_EQ (VersionCompare ("2026.11.2", "2026.11.2"), VersionSame);
	CHECK_EQ (VersionCompare ("2026.11.2", "2026.11.1"), VersionOlder);
	CHECK_EQ (VersionCompare ("2026.11.10", "2026.11.9"), VersionOlder);	// numeric, not text
	CHECK_EQ (VersionCompare ("2026.12.9", "2027.01.0"), VersionNewer);
}

TEST (VersionCompareNotComparable)
{
	CHECK_EQ (VersionCompare ("dev", "2026.11.2"), VersionNotComparable);
	CHECK_EQ (VersionCompare ("2026.11.2", "dev"), VersionNotComparable);
	CHECK_EQ (VersionCompare ("2026.11.2", "2026.13.0"), VersionNotComparable);
	CHECK_EQ (VersionCompare ("2026.11.2", "2026.1.0"), VersionNotComparable);
	CHECK_EQ (VersionCompare ("2026.11.2", "2026.11.01"), VersionNotComparable);
	CHECK_EQ (VersionCompare ("2026.11.2", "2026.11.2-rc1"), VersionNotComparable);
	CHECK_EQ (VersionCompare ("2026.11.2", "v2026.11.3"), VersionNotComparable);
}

// --- Form data ------------------------------------------------------------------------

TEST (FormDataDecodes)
{
	char Value[32];
	const char *pData = "mqtt_host=192.168.1.10&wifi_ssid=My+Home%21&empty=&flag";
	CHECK_EQ (FormDataGet (pData, "mqtt_host", Value, sizeof Value), FormFound);
	CHECK_STR (Value, "192.168.1.10");
	CHECK_EQ (FormDataGet (pData, "wifi_ssid", Value, sizeof Value), FormFound);
	CHECK_STR (Value, "My Home!");
	CHECK_EQ (FormDataGet (pData, "empty", Value, sizeof Value), FormFound);
	CHECK_STR (Value, "");
	CHECK_EQ (FormDataGet (pData, "flag", Value, sizeof Value), FormFound);
	CHECK_EQ (FormDataGet (pData, "mqtt", Value, sizeof Value), FormMissing);	// no prefix match
	CHECK_EQ (FormDataGet ("", "a", Value, sizeof Value), FormMissing);
}

TEST (FormDataRejectsBadInput)
{
	char Value[4];
	CHECK_EQ (FormDataGet ("a=12345", "a", Value, sizeof Value), FormTooLong);
	CHECK_EQ (FormDataGet ("a=%4", "a", Value, sizeof Value), FormInvalid);
	CHECK_EQ (FormDataGet ("a=%zz", "a", Value, sizeof Value), FormInvalid);
	CHECK_EQ (FormDataGet ("a=x%0Ay", "a", Value, sizeof Value), FormInvalid);	// LF
	CHECK_EQ (FormDataGet ("a=%00", "a", Value, sizeof Value), FormInvalid);
	CHECK_EQ (FormDataGet ("a=1&a=2", "a", Value, sizeof Value), FormFound);
	CHECK_STR (Value, "1");							// first wins
}

// --- JSON writer ----------------------------------------------------------------------

TEST (JSONWriterBuildsObjects)
{
	char Buffer[256];
	CJSONWriter J (Buffer, sizeof Buffer);
	J.BeginObject ();
	J.String ("result", "error");
	J.String ("message", "bad \"value\"\n");
	J.Number ("uptime_s", 42);
	J.Bool ("update_enabled", false);
	J.BeginArray ("fields");
	J.String (0, "mqtt_port");
	J.String (0, "wifi_country");
	J.EndArray ();
	J.BeginObject ("nested");
	J.EndObject ();
	J.EndObject ();
	CHECK (!J.Overflow ());
	CHECK_STR (J.Get (), "{\"result\":\"error\",\"message\":\"bad \\\"value\\\"\\u000a\","
			     "\"uptime_s\":42,\"update_enabled\":false,"
			     "\"fields\":[\"mqtt_port\",\"wifi_country\"],\"nested\":{}}");
}

TEST (JSONWriterOverflow)
{
	char Buffer[8];
	CJSONWriter J (Buffer, sizeof Buffer);
	J.BeginObject ();
	J.String ("result", "installed");
	J.EndObject ();
	CHECK (J.Overflow ());
}
