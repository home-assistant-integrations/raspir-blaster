#!/usr/bin/env bash
#
#  firmware-lib.sh
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

# Shared helpers for the boot/Wi-Fi firmware manifest (scripts/firmware-files.txt).
# Sourced by scripts/sdcard.sh and scripts/firmware-checksums.sh; expects
# ROOT_DIR to be set and scripts/versions.env to be sourced.

FIRMWARE_MANIFEST="$ROOT_DIR/scripts/firmware-files.txt"
FIRMWARE_CACHE="$ROOT_DIR/toolchain/downloads"

# Prints the manifest entries without comments and blank lines:
#   sha256 source path destination outputs
firmware_entries() {
    grep -vE '^[[:space:]]*(#|$)' "$FIRMWARE_MANIFEST"
}

# firmware_url <source> <path>
firmware_url() {
    case "$1" in
        boot) printf '%s/%s' "$RPI_FIRMWARE_URL" "$2" ;;
        wifi) printf '%s/%s' "$RPI_WIFI_FIRMWARE_URL" "$2" ;;
        *)    printf '[firmware] ERROR: unknown source %s\n' "$1" >&2; return 1 ;;
    esac
}

# firmware_cache_file <source> <path>
firmware_cache_file() {
    case "$1" in
        boot) printf '%s/rpi-firmware-%s/%s' "$FIRMWARE_CACHE" "$RPI_FIRMWARE_COMMIT" "$2" ;;
        wifi) printf '%s/rpi-wifi-firmware-%s/%s' "$FIRMWARE_CACHE" "$RPI_WIFI_FIRMWARE_COMMIT" "$2" ;;
    esac
}

# firmware_download <url> <file>: downloads unless the file is already cached.
firmware_download() {
    if [[ -s "$2" ]]; then
        return 0
    fi

    printf '[firmware] downloading %s\n' "$1" >&2
    mkdir -p "$(dirname "$2")"
    curl --fail --silent --show-error --location --output "$2.part" "$1"
    mv "$2.part" "$2"
}

firmware_sha256() {
    shasum -a 256 "$1" | awk '{print $1}'
}
