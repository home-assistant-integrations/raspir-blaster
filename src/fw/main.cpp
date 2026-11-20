/*
 *  main.cpp
 *  raspir-blaster firmware
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

#include "kernel.h"
#include <circle/startup.h>

int main (void)
{
	// Must not return: some destructors used by CKernel are not implemented.
	CKernel Kernel;
	if (!Kernel.Initialize ())
	{
		halt ();
		return EXIT_HALT;
	}

	Kernel.Run ();

	halt ();
	return EXIT_HALT;
}
