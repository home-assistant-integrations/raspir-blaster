/*
 *  version.h
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

#ifndef _raspir_version_h
#define _raspir_version_h

#define RASPIR_FIRMWARE_NAME    "raspir-blaster"

// The firmware build generates build/generated/firmware_version.h from the
// VERSION make variable or the VERSION file (release branches). Builds without
// it, e.g. the host unit tests, report "dev".
#if __has_include("firmware_version.h")
	#include "firmware_version.h"
#endif

#ifndef RASPIR_FIRMWARE_VERSION
	#define RASPIR_FIRMWARE_VERSION "dev"
#endif

#endif
