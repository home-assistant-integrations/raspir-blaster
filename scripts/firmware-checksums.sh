#!/usr/bin/env bash
#
#  firmware-checksums.sh
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

# Downloads every file of scripts/firmware-files.txt at the commits pinned in
# scripts/versions.env and writes its SHA-256 into the first column of the
# manifest. Run this only after deliberately changing a pinned commit, then
# review and commit the updated manifest.

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
# shellcheck source=versions.env
source "$ROOT_DIR/scripts/versions.env"
# shellcheck source=firmware-lib.sh
source "$ROOT_DIR/scripts/firmware-lib.sh"

TMP_MANIFEST="$(mktemp "${TMPDIR:-/tmp}/firmware-files.XXXXXX")"
trap 'rm -f "$TMP_MANIFEST"' EXIT

while IFS= read -r line; do
    if [[ "$line" =~ ^[[:space:]]*(#|$) ]]; then
        printf '%s\n' "$line"
        continue
    fi

    read -r _ source path destination outputs <<< "$line"
    cache_file="$(firmware_cache_file "$source" "$path")"

    # Always fetch fresh: the cache may hold a file from an older checksum run.
    rm -f "$cache_file"
    firmware_download "$(firmware_url "$source" "$path")" "$cache_file"

    printf '%s  %-4s  %-36s  %-38s  %s\n' \
        "$(firmware_sha256 "$cache_file")" "$source" "$path" "$destination" "$outputs"
done < "$FIRMWARE_MANIFEST" > "$TMP_MANIFEST"

cp "$TMP_MANIFEST" "$FIRMWARE_MANIFEST"
printf '[firmware] checksums written to %s\n' "${FIRMWARE_MANIFEST#$ROOT_DIR/}"
