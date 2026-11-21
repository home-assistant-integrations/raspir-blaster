/*
 *  netselect.cpp
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

#include "netselect.h"

TNetSelection NetSelect (bool bHasEthernet, bool bWiFiConfig, bool bWiFiMarker)
{
	TNetSelection Selection;

	// A marker is only ever created on an Ethernet board with a Wi-Fi config.
	// Whatever the situation now, it is used at most once.
	Selection.bDeleteMarker = bWiFiMarker;

	if (!bHasEthernet)
	{
		Selection.Choice = bWiFiConfig ? NetChoiceWiFi : NetChoiceNone;
		Selection.Reason = bWiFiConfig ? NetReasonNoEthernetPort : NetReasonNothingConfigured;
	}
	else if (!bWiFiConfig)
	{
		Selection.Choice = NetChoiceEthernet;
		Selection.Reason = NetReasonNoWiFiConfig;
	}
	else if (bWiFiMarker)
	{
		Selection.Choice = NetChoiceWiFi;
		Selection.Reason = NetReasonWiFiMarker;
	}
	else
	{
		Selection.Choice = NetChoiceProbe;
		Selection.Reason = NetReasonCheckLink;
	}

	return Selection;
}

TNetProbeResult NetSelectAfterProbe (bool bLinkUp, bool bProbeBeforeNet)
{
	if (bLinkUp)
	{
		return NetProbeEthernet;
	}

	return bProbeBeforeNet ? NetProbeWiFi : NetProbeMarkAndReboot;
}
