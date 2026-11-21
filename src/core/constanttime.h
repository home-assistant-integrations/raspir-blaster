/*
 *  constanttime.h
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

#ifndef _raspir_constanttime_h
#define _raspir_constanttime_h

#include <stddef.h>

/// Compares two NUL-terminated strings without branching on their content,
/// so comparing a secret (e.g. ota_password) does not leak timing
/// information through an early mismatch. Lengths are not kept secret.
bool ConstantTimeEqual (const char *pA, const char *pB);

#endif
