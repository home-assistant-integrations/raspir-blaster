#!/usr/bin/env bash
#
#  next-version.sh
#  raspir-blaster
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

# Prints the next release version yyyy.mm.rm (see openspec/specs/release-process).
#
# Reads existing ref names from stdin, as printed by
#   git ls-remote origin 'refs/heads/rel-*' 'refs/tags/*'
# (lines "<sha>\t<ref>" or just "<ref>"). yyyy.mm is the current UTC month,
# or RELEASE_MONTH (format yyyy.mm) if set. rm is one higher than the highest
# release of that month found in refs/heads/rel-<yyyy.mm>.<rm> or
# refs/tags/<yyyy.mm>.<rm>, or 0 if there is none.

set -euo pipefail

month="${RELEASE_MONTH:-$(date -u +%Y.%m)}"
if [[ ! "$month" =~ ^[0-9]{4}\.(0[1-9]|1[0-2])$ ]]; then
    printf 'next-version: invalid month "%s" (expected yyyy.mm)\n' "$month" >&2
    exit 1
fi

highest=-1
while read -r first second _; do
    ref="${second:-$first}"
    case "$ref" in
        refs/heads/rel-"$month".*) rm="${ref#refs/heads/rel-$month.}" ;;
        refs/tags/"$month".*)      rm="${ref#refs/tags/$month.}" ;;
        *)                         continue ;;
    esac
    # Only plain counters count; ignore e.g. peeled tags ("^{}") or suffixes.
    if [[ "$rm" =~ ^(0|[1-9][0-9]*)$ ]] && (( rm > highest )); then
        highest=$rm
    fi
done

printf '%s.%d\n' "$month" $(( highest + 1 ))
