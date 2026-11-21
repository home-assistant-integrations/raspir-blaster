/*
 *  netselect.h
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

#ifndef _raspir_netselect_h
#define _raspir_netselect_h

// Network interface selection at boot (openspec/specs/connectivity,
// "Network interface selection"). Only one interface is used per boot.

#define ETHERNET_LINK_TIMEOUT_MS	5000	// link check after the Ethernet chip is up
#define WIFI_MARKER_FILE_NAME		"raspir-blaster.wifi"

enum TNetChoice
{
	NetChoiceWiFi,		// use Wi-Fi without an Ethernet check
	NetChoiceEthernet,	// use Ethernet without a link check (waits for a cable)
	NetChoiceProbe,		// check the Ethernet link, then NetSelectAfterProbe()
	NetChoiceNone		// no usable interface: degraded state
};

enum TNetReason
{
	NetReasonNoEthernetPort,	// board has no Ethernet: Wi-Fi
	NetReasonNoWiFiConfig,		// Ethernet board without wpa_supplicant.conf: Ethernet
	NetReasonWiFiMarker,		// marker from the previous boot: Wi-Fi
	NetReasonCheckLink,		// Ethernet board with Wi-Fi config: check the link
	NetReasonNothingConfigured	// no Ethernet and no wpa_supplicant.conf
};

struct TNetSelection
{
	TNetChoice	Choice;
	TNetReason	Reason;
	bool		bDeleteMarker;	// a marker file exists and must be deleted first
};

/// \param bHasEthernet	board has an Ethernet port (Pi 3 B/B+, Pi 4 B, CM4)
/// \param bWiFiConfig	wpa_supplicant.conf exists
/// \param bWiFiMarker	the marker file of a previous boot exists
TNetSelection NetSelect (bool bHasEthernet, bool bWiFiConfig, bool bWiFiMarker);

enum TNetProbeResult
{
	NetProbeEthernet,	// link is up: use Ethernet
	NetProbeWiFi,		// no link, Wi-Fi can still be set up in this boot
	NetProbeMarkAndReboot	// no link, network already on Ethernet: marker + reboot
};

/// \param bLinkUp		the Ethernet link came up within ETHERNET_LINK_TIMEOUT_MS
/// \param bProbeBeforeNet	the link was checked before the network was set up (Pi 3)
TNetProbeResult NetSelectAfterProbe (bool bLinkUp, bool bProbeBeforeNet);

#endif
