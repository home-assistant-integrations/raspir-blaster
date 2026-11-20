#!/usr/bin/env bash
#
#  package-release.sh
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

# Packages the release assets after "make sdcard":
#
#   scripts/package-release.sh <version>
#
#   build/release/raspir-blaster-<version>-sdcard.zip
#   build/release/raspir-blaster-<version>-cm4-emmc.zip
#   build/release/SHA256SUMS.txt
#
# The zips contain the files at the top level, ready to copy onto the boot media.

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="$ROOT_DIR/build"
RELEASE_DIR="$BUILD_DIR/release"

fail() { printf '[package] ERROR: %s\n' "$*" >&2; exit 1; }

[[ $# -eq 1 ]] || fail "usage: $0 <version>"
version="$1"

for dir in sdcard cm4-emmc; do
    [[ -f "$BUILD_DIR/$dir/config.txt" ]] || fail "build/$dir/ missing - run 'make sdcard' first"
done

# The images must carry the version being released.
built="$(sed -n 's/^#define RASPIR_FIRMWARE_VERSION "\(.*\)"$/\1/p' \
         "$BUILD_DIR/generated/firmware_version.h" 2>/dev/null || true)"
[[ "$built" == "$version" ]] \
    || fail "firmware was built with version '${built:-unknown}', not '$version'"

rm -rf "$RELEASE_DIR"
mkdir -p "$RELEASE_DIR"

for dir in sdcard cm4-emmc; do
    archive="raspir-blaster-$version-$dir.zip"
    (cd "$BUILD_DIR/$dir" && zip -r -X -q "$RELEASE_DIR/$archive" .)
    printf '[package] %s\n' "$archive"
done

(cd "$RELEASE_DIR" && shasum -a 256 ./*.zip | sed 's| \./| |' > SHA256SUMS.txt)
printf '[package] SHA256SUMS.txt\n'
