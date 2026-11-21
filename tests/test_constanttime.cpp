/*
 *  test_constanttime.cpp
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

#include "test.h"
#include "constanttime.h"

TEST (ConstantTimeEqualAcceptsIdentical)
{
	CHECK (ConstantTimeEqual ("secret123", "secret123"));
	CHECK (ConstantTimeEqual ("", ""));
	CHECK (ConstantTimeEqual ("a", "a"));
}

TEST (ConstantTimeEqualRejectsDifferentContent)
{
	CHECK (!ConstantTimeEqual ("secret123", "secret124"));
	CHECK (!ConstantTimeEqual ("secret123", "Secret123"));
	CHECK (!ConstantTimeEqual ("a", "b"));
}

TEST (ConstantTimeEqualRejectsDifferentLength)
{
	CHECK (!ConstantTimeEqual ("secret", "secret1"));
	CHECK (!ConstantTimeEqual ("secret1", "secret"));
	CHECK (!ConstantTimeEqual ("", "x"));
	CHECK (!ConstantTimeEqual ("x", ""));
}

TEST (ConstantTimeEqualRejectsPrefixMatch)
{
	// A common wrong-implementation bug: stopping at the shorter string's
	// NUL should not report equality.
	CHECK (!ConstantTimeEqual ("pass", "password"));
	CHECK (!ConstantTimeEqual ("password", "pass"));
}
