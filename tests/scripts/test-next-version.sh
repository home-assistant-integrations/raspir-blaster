#!/usr/bin/env bash
#
#  test-next-version.sh
#  raspir-blaster tests
#
#  Created by Thomas Bonk on 2026-11-20.
#  Copyright 2026 Thomas Bonk <thomas@meandmymac.de>
#
#  Licensed under the Apache License, Version 2.0 (the "License");
#  you may not use this file except in compliance with the License.
#  You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
#  Unless required by applicable law or agreed to in writing, software
#  distributed under the License is distributed on an "AS IS" BASIS,
#  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#  See the License for the specific language governing permissions and
#  limitations under the License.
#

# Tests for scripts/next-version.sh. Run by "make test".

set -uo pipefail

SCRIPT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)/scripts/next-version.sh"
failures=0
count=0

# check <name> <month> <expected> <refs...>
check() {
    local name="$1" month="$2" expected="$3"
    shift 3
    local actual
    actual="$(printf '%s\n' "$@" | RELEASE_MONTH="$month" "$SCRIPT" 2>&1)"
    count=$((count + 1))
    if [[ "$actual" != "$expected" ]]; then
        printf '[FAIL] %s: expected "%s", got "%s"\n' "$name" "$expected" "$actual"
        failures=$((failures + 1))
    fi
}

SHA=0123456789abcdef0123456789abcdef01234567

check "first release of a month"     2026.11 2026.11.0
check "first release, empty input"   2026.11 2026.11.0 ""
check "increment"                    2026.11 2026.11.2 \
      "$SHA	refs/heads/rel-2026.11.0" "$SHA	refs/heads/rel-2026.11.1"
check "new month resets"             2026.12 2026.12.0 \
      "$SHA	refs/heads/rel-2026.11.4" "$SHA	refs/tags/2026.11.4"
check "gap uses highest"             2026.11 2026.11.4 \
      "$SHA	refs/heads/rel-2026.11.0" "$SHA	refs/heads/rel-2026.11.3"
check "tags count"                   2026.11 2026.11.6 \
      "$SHA	refs/tags/2026.11.5" "$SHA	refs/tags/2026.11.5^{}"
check "two-digit counter"            2026.11 2026.11.11 \
      "$SHA	refs/heads/rel-2026.11.9" "$SHA	refs/heads/rel-2026.11.10"
check "unrelated refs ignored"       2026.11 2026.11.0 \
      "$SHA	refs/heads/main" "$SHA	refs/heads/rel-2026.10.7" \
      "$SHA	refs/tags/v2026.11.3" "$SHA	refs/heads/rel-2026.11.x" \
      "$SHA	refs/heads/rel-2026.11.01" "$SHA	refs/heads/rel-2026.110.2"
check "refs without sha column"      2026.01 2026.01.1 "refs/heads/rel-2026.01.0"
check "invalid month rejected"       2026.13 'next-version: invalid month "2026.13" (expected yyyy.mm)'

# Default month comes from the UTC date.
default="$(printf '' | env -u RELEASE_MONTH "$SCRIPT")"
count=$((count + 1))
if [[ "$default" != "$(date -u +%Y.%m).0" ]]; then
    printf '[FAIL] default month: got "%s"\n' "$default"
    failures=$((failures + 1))
fi

printf '%d script tests, %d failed\n' "$count" "$failures"
[[ $failures -eq 0 ]]
