#!/usr/bin/env bash
#
#  sdcard.sh
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

# Assembles the boot media directories (see openspec/specs/board-support):
#
#   build/sdcard/    universal SD card: Zero W, Zero 2 W, Pi 3, Pi 4, CM4 Lite
#   build/cm4-emmc/  boot files for a Compute Module 4 with eMMC
#
# Boot files and Wi-Fi firmware are downloaded from the Raspberry Pi GitHub
# repositories at the commits pinned in scripts/versions.env and verified
# against scripts/firmware-files.txt. Downloads are cached in toolchain/downloads/.

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
# shellcheck source=versions.env
source "$ROOT_DIR/scripts/versions.env"
# shellcheck source=firmware-lib.sh
source "$ROOT_DIR/scripts/firmware-lib.sh"

IMAGES_DIR="$ROOT_DIR/build/images"
SDCARD_DIR="$ROOT_DIR/build/sdcard"
EMMC_DIR="$ROOT_DIR/build/cm4-emmc"

log()  { printf '[sdcard] %s\n' "$*"; }
fail() { printf '[sdcard] ERROR: %s\n' "$*" >&2; exit 1; }

# Kernel images per output directory: "<target>:<image>"
SDCARD_IMAGES="rpi0w:kernel.img rpi3:kernel8.img rpi4:kernel8-rpi4.img"
EMMC_IMAGES="rpi4-emmc:kernel8-rpi4.img"

# --- Preconditions ----------------------------------------------------------

missing=""
for entry in $SDCARD_IMAGES $EMMC_IMAGES; do
    [[ -f "$IMAGES_DIR/${entry%%:*}/${entry#*:}" ]] || missing+=" ${entry%%:*}"
done
[[ -z "$missing" ]] \
    || fail "kernel image missing for target(s):$missing - run 'make' (all targets) first"

ARMSTUB=""
for tree in circle-rpi4 circle-rpi4-emmc; do
    candidate="$ROOT_DIR/toolchain/$tree/boot/armstub/armstub8-rpi4.bin"
    if [[ -f "$candidate" ]]; then
        ARMSTUB="$candidate"
        break
    fi
done
[[ -n "$ARMSTUB" ]] || fail "armstub8-rpi4.bin missing - run 'make' first"

# --- Download and verify boot files and Wi-Fi firmware ---------------------

while read -r sha source path destination outputs; do
    cache_file="$(firmware_cache_file "$source" "$path")"
    firmware_download "$(firmware_url "$source" "$path")" "$cache_file"

    actual="$(firmware_sha256 "$cache_file")"
    if [[ "$actual" != "$sha" ]]; then
        rm -f "$cache_file"
        fail "checksum mismatch for $destination ($source:$path): expected $sha, got $actual; download deleted"
    fi
done < <(firmware_entries)

# --- Assemble ---------------------------------------------------------------

# assemble <output name> <directory> <config.txt source> <images>
assemble() {
    local name="$1" dir="$2" config="$3" images="$4"

    log "assembling ${dir#$ROOT_DIR/}"
    rm -rf "$dir"
    mkdir -p "$dir/firmware"

    while read -r _ source path destination outputs; do
        if [[ ",$outputs," == *",$name,"* ]]; then
            mkdir -p "$(dirname "$dir/$destination")"
            cp "$(firmware_cache_file "$source" "$path")" "$dir/$destination"
        fi
    done < <(firmware_entries)

    for entry in $images; do
        cp "$IMAGES_DIR/${entry%%:*}/${entry#*:}" "$dir/${entry#*:}"
    done

    cp "$ARMSTUB" "$dir/armstub8-rpi4.bin"
    cp "$config" "$dir/config.txt"
    cp "$ROOT_DIR/config/raspir-blaster.conf.example" "$dir/"
    cp "$ROOT_DIR/config/wpa_supplicant.conf.example" "$dir/"
}

assemble sdcard   "$SDCARD_DIR" "$ROOT_DIR/config/config.txt"           "$SDCARD_IMAGES"
assemble cm4-emmc "$EMMC_DIR"   "$ROOT_DIR/config/config-cm4-emmc.txt" "$EMMC_IMAGES"

log "done:"
log "  build/sdcard/    copy to a FAT32 SD card (Zero W, Zero 2 W, Pi 3, Pi 4, CM4 Lite)"
log "  build/cm4-emmc/  copy to the boot partition of a CM4's eMMC (via rpiboot)"
log "then rename and edit raspir-blaster.conf.example and wpa_supplicant.conf.example"
