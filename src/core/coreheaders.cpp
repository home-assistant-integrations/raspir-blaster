/*
 *  coreheaders.cpp
 *  raspir-blaster core
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

// Includes every src/core header. This file is compiled by both the host test
// build (without libc++ headers) and the Circle firmware build, so a core
// header that pulls in a host-only or Circle-only dependency breaks one of
// the two builds.

#include "backoff.h"	// IWYU pragma: keep
#include "config.h"	// IWYU pragma: keep
#include "constanttime.h"	// IWYU pragma: keep
#include "discovery.h"	// IWYU pragma: keep
#include "configwriter.h"	// IWYU pragma: keep
#include "dhcpserver.h"	// IWYU pragma: keep
#include "formdata.h"	// IWYU pragma: keep
#include "inflate.h"	// IWYU pragma: keep
#include "ircarrier.h"	// IWYU pragma: keep
#include "zipreader.h"	// IWYU pragma: keep
#include "irframeassembler.h"	// IWYU pragma: keep
#include "irmessage.h"	// IWYU pragma: keep
#include "irtxqueue.h"	// IWYU pragma: keep
#include "jsontokenizer.h"	// IWYU pragma: keep
#include "jsonwriter.h"	// IWYU pragma: keep
#include "logring.h"	// IWYU pragma: keep
#include "manifest.h"	// IWYU pragma: keep
#include "netselect.h"	// IWYU pragma: keep
#include "sha256.h"	// IWYU pragma: keep
#include "textwriter.h"	// IWYU pragma: keep
#include "version.h"	// IWYU pragma: keep
#include "wpareader.h"	// IWYU pragma: keep
