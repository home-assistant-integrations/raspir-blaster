/*
 *  test_netselect.cpp
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

// Tests for the network interface selection (connectivity spec,
// "Network interface selection").

#include "test.h"
#include "netselect.h"

// --- table rows: NetSelect (bHasEthernet, bWiFiConfig, bWiFiMarker) -------------

TEST (NetSelectNoEthernetWithWiFiConfig)
{
	TNetSelection S = NetSelect (false, true, false);
	CHECK_EQ (S.Choice, NetChoiceWiFi);
	CHECK_EQ (S.Reason, NetReasonNoEthernetPort);
	CHECK (!S.bDeleteMarker);
}

TEST (NetSelectNoEthernetNoWiFiConfigIsDegraded)
{
	TNetSelection S = NetSelect (false, false, false);
	CHECK_EQ (S.Choice, NetChoiceNone);
	CHECK_EQ (S.Reason, NetReasonNothingConfigured);
}

TEST (NetSelectEthernetWithoutWiFiConfigSkipsLinkCheck)
{
	TNetSelection S = NetSelect (true, false, false);
	CHECK_EQ (S.Choice, NetChoiceEthernet);
	CHECK_EQ (S.Reason, NetReasonNoWiFiConfig);
	CHECK (!S.bDeleteMarker);
}

TEST (NetSelectEthernetWithWiFiConfigChecksLink)
{
	TNetSelection S = NetSelect (true, true, false);
	CHECK_EQ (S.Choice, NetChoiceProbe);
	CHECK_EQ (S.Reason, NetReasonCheckLink);
	CHECK (!S.bDeleteMarker);
}

// --- marker file ----------------------------------------------------------------

TEST (NetSelectMarkerUsesWiFiOnceAndIsDeleted)
{
	TNetSelection S = NetSelect (true, true, true);
	CHECK_EQ (S.Choice, NetChoiceWiFi);
	CHECK_EQ (S.Reason, NetReasonWiFiMarker);
	CHECK (S.bDeleteMarker);

	// Next boot without the marker checks the link again.
	S = NetSelect (true, true, false);
	CHECK_EQ (S.Choice, NetChoiceProbe);
}

TEST (NetSelectMarkerWithoutWiFiConfigIsIgnored)
{
	// wpa_supplicant.conf removed after the marker was written: Ethernet,
	// and the stale marker is still deleted.
	TNetSelection S = NetSelect (true, false, true);
	CHECK_EQ (S.Choice, NetChoiceEthernet);
	CHECK_EQ (S.Reason, NetReasonNoWiFiConfig);
	CHECK (S.bDeleteMarker);
}

TEST (NetSelectMarkerOnBoardWithoutEthernetIsDeleted)
{
	// Card moved from a Pi 4 to a Zero W between the two boots.
	TNetSelection S = NetSelect (false, true, true);
	CHECK_EQ (S.Choice, NetChoiceWiFi);
	CHECK_EQ (S.Reason, NetReasonNoEthernetPort);
	CHECK (S.bDeleteMarker);
}

// --- after the link check: NetSelectAfterProbe (bLinkUp, bProbeBeforeNet) -------

TEST (NetProbeLinkUpUsesEthernet)
{
	CHECK_EQ (NetSelectAfterProbe (true, true), NetProbeEthernet);	// Pi 3 B/B+
	CHECK_EQ (NetSelectAfterProbe (true, false), NetProbeEthernet);	// Pi 4 / CM4
}

TEST (NetProbeNoLinkPi3FallsBackWithoutReboot)
{
	CHECK_EQ (NetSelectAfterProbe (false, true), NetProbeWiFi);
}

TEST (NetProbeNoLinkPi4WritesMarkerAndReboots)
{
	CHECK_EQ (NetSelectAfterProbe (false, false), NetProbeMarkAndReboot);
}
