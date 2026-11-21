/*
 *  formdata.h
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

#ifndef _raspir_formdata_h
#define _raspir_formdata_h

#include <stddef.h>

enum TFormResult
{
	FormFound,
	FormMissing,		// no field with this name
	FormTooLong,		// decoded value does not fit (incl. NUL)
	FormInvalid		// bad %-escape or a decoded NUL/control character
};

/// Looks up a field in application/x-www-form-urlencoded data
/// ("a=1&b=x%20y"), decodes "+" and "%xx", and copies it NUL-terminated.
/// The first field with this name wins. Decoded control characters
/// (< 0x20, 0x7F) are refused, so values can go into text files.
TFormResult FormDataGet (const char *pData, const char *pName, char *pValue, size_t nSize);

#endif
