#!/usr/bin/env bash
#
#  bootstrap.sh
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

# Downloads the pinned ARM GNU Toolchains and Circle into toolchain/ and builds
# one Circle tree per build target.
#
#   scripts/bootstrap.sh [target ...]     (default: all targets)
#
# Targets: rpi0w rpi3 rpi4 rpi4-emmc (see openspec/specs/board-support).
# Idempotent: every step is guarded by a stamp file that encodes the pinned
# versions.

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
TOOLCHAIN_ROOT="$ROOT_DIR/toolchain"
ALL_TARGETS="rpi0w rpi3 rpi4 rpi4-emmc"

# shellcheck source=versions.env
source "$ROOT_DIR/scripts/versions.env"

log()  { printf '[bootstrap] %s\n' "$*"; }
fail() { printf '[bootstrap] ERROR: %s\n' "$*" >&2; exit 1; }

check_host() {
    local os arch
    os="$(uname -s)"
    arch="$(uname -m)"
    [[ "$os" == "Darwin" && "$arch" == "arm64" ]] \
        || fail "unsupported build host $os/$arch: the pinned toolchain ($TOOLCHAIN_VERSION) is only available for macOS on Apple Silicon"
}

# --- Targets ----------------------------------------------------------------

# target_config <target>: sets T_RASPPI, T_AARCH, T_TRIPLE, T_DEFINES
target_config() {
    T_DEFINES=""
    case "$1" in
        rpi0w)      T_RASPPI=1; T_AARCH=32; T_TRIPLE=arm-none-eabi ;;
        rpi3)       T_RASPPI=3; T_AARCH=64; T_TRIPLE=aarch64-none-elf ;;
        rpi4)       T_RASPPI=4; T_AARCH=64; T_TRIPLE=aarch64-none-elf ;;
        rpi4-emmc)  T_RASPPI=4; T_AARCH=64; T_TRIPLE=aarch64-none-elf
                    T_DEFINES="-DUSE_EMBEDDED_MMC_CM" ;;
        *)          fail "unknown target '$1' (valid targets: $ALL_TARGETS)" ;;
    esac
}

# --- Toolchains -------------------------------------------------------------

toolchain_dir() {
    printf '%s/arm-gnu-toolchain-%s-%s-%s' "$TOOLCHAIN_ROOT" "$TOOLCHAIN_VERSION" "$TOOLCHAIN_HOST" "$1"
}

# install_toolchain <triple>
install_toolchain() {
    local triple="$1"
    local name="arm-gnu-toolchain-${TOOLCHAIN_VERSION}-${TOOLCHAIN_HOST}-${triple}"
    local dir stamp
    dir="$(toolchain_dir "$triple")"
    stamp="$TOOLCHAIN_ROOT/.stamp-toolchain-$TOOLCHAIN_VERSION-$triple"

    if [[ -f "$stamp" && -x "$dir/bin/${triple}-gcc" ]]; then
        log "toolchain $triple $TOOLCHAIN_VERSION already installed"
        return
    fi

    local archive="$name.tar.xz"
    local url="$TOOLCHAIN_URL_BASE/$TOOLCHAIN_VERSION/binrel/$archive"
    local download_dir="$TOOLCHAIN_ROOT/downloads"
    local expected_var="TOOLCHAIN_SHA256_${triple//-/_}"
    local expected="${!expected_var:-}"

    [[ -n "$expected" ]] || fail "no SHA-256 recorded for $triple in scripts/versions.env"

    mkdir -p "$download_dir"
    if [[ ! -f "$download_dir/$archive" ]]; then
        log "downloading $archive"
        curl --fail --location --progress-bar --output "$download_dir/$archive.part" "$url"
        mv "$download_dir/$archive.part" "$download_dir/$archive"
    fi

    log "verifying SHA-256 of $archive"
    local actual
    actual="$(shasum -a 256 "$download_dir/$archive" | awk '{print $1}')"
    if [[ "$actual" != "$expected" ]]; then
        rm -f "$download_dir/$archive"
        fail "checksum mismatch for $archive (expected $expected, got $actual); archive deleted"
    fi

    log "extracting $triple toolchain"
    local tmp_dir="$TOOLCHAIN_ROOT/.extract.$$"
    rm -rf "$tmp_dir" "$dir"
    mkdir -p "$tmp_dir"
    tar -xJf "$download_dir/$archive" -C "$tmp_dir"
    mv "$tmp_dir/$name" "$dir"
    rm -rf "$tmp_dir"

    "$dir/bin/${triple}-gcc" --version | head -1
    touch "$stamp"
}

# --- Pristine Circle checkout -----------------------------------------------

CIRCLE_DIR="$TOOLCHAIN_ROOT/circle"
CIRCLE_STAMP="$TOOLCHAIN_ROOT/.stamp-circle-$CIRCLE_TAG"

install_circle() {
    if [[ -f "$CIRCLE_STAMP" && -d "$CIRCLE_DIR/.git" ]]; then
        log "Circle $CIRCLE_TAG already cloned"
    else
        log "cloning Circle $CIRCLE_TAG"
        rm -rf "$CIRCLE_DIR" "$TOOLCHAIN_ROOT"/circle-* "$TOOLCHAIN_ROOT"/.stamp-circle-* \
               "$TOOLCHAIN_ROOT"/.stamp-tree-* "$TOOLCHAIN_ROOT"/.stamp-libs-*
        git -c advice.detachedHead=false clone --quiet --depth 1 --branch "$CIRCLE_TAG" \
            "$CIRCLE_REPO" "$CIRCLE_DIR"
        # The WPA supplicant (hostap) is a git submodule of Circle.
        git -C "$CIRCLE_DIR" submodule --quiet update --init --depth 1 addon/wlan/hostap
        touch "$CIRCLE_STAMP"
    fi

    # Older layouts built Circle inside the checkout itself. The checkout must
    # stay pristine, because it is the source of all per-target trees.
    if [[ -f "$CIRCLE_DIR/Config.mk" || -f "$CIRCLE_DIR/lib/libcircle.a" ]]; then
        log "cleaning build outputs from the pristine Circle checkout"
        git -C "$CIRCLE_DIR" clean -fdxq
        git -C "$CIRCLE_DIR" submodule --quiet foreach --recursive git clean -fdxq
        rm -f "$TOOLCHAIN_ROOT"/.stamp-libs-"$CIRCLE_TAG"-*
    fi
}

# --- Per-target Circle trees ------------------------------------------------

# install_tree <target>: APFS clone of the pristine checkout plus Config.mk
install_tree() {
    local target="$1"
    local tree="$TOOLCHAIN_ROOT/circle-$target"
    local stamp="$TOOLCHAIN_ROOT/.stamp-tree-$target-$CIRCLE_TAG"

    if [[ ! -f "$stamp" || ! -d "$tree" ]]; then
        log "creating Circle tree for $target"
        rm -rf "$tree" "$TOOLCHAIN_ROOT"/.stamp-libs-"$target"-*
        # -c clones the files (copy-on-write, no extra space on APFS).
        cp -Rc "$CIRCLE_DIR" "$tree" 2>/dev/null || cp -R "$CIRCLE_DIR" "$tree"
        touch "$stamp"
    fi

    target_config "$target"
    local prefix_var=PREFIX
    [[ "$T_AARCH" == 64 ]] && prefix_var=PREFIX64

    # Always (re)write Config.mk so it follows the toolchain location.
    local config
    config="$(cat <<EOF
# Generated by scripts/bootstrap.sh for target $target - do not edit.
RASPPI = $T_RASPPI
AARCH = $T_AARCH
$prefix_var = $(toolchain_dir "$T_TRIPLE")/bin/${T_TRIPLE}-
EOF
)"
    if [[ -n "$T_DEFINES" ]]; then
        config+=$'\n'"DEFINE += $T_DEFINES"
    fi

    # Rewrite only on change, so make does not see a newer Config.mk.
    if [[ ! -f "$tree/Config.mk" ]] || [[ "$(cat "$tree/Config.mk")" != "$config" ]]; then
        printf '%s\n' "$config" > "$tree/Config.mk"
        rm -f "$TOOLCHAIN_ROOT"/.stamp-libs-"$target"-*
    fi
}

# build_libs <target>
build_libs() {
    local target="$1"
    local tree="$TOOLCHAIN_ROOT/circle-$target"
    local stamp="$TOOLCHAIN_ROOT/.stamp-libs-$target-$CIRCLE_TAG-$TOOLCHAIN_VERSION"

    if [[ -f "$stamp" ]]; then
        log "Circle libraries for $target already built"
        return
    fi

    local jobs
    jobs="$(sysctl -n hw.ncpu)"

    log "building Circle libraries for $target"
    (cd "$tree" && ./makeall --nosample -j"$jobs")
    make -C "$tree/addon/SDCard" -j"$jobs"
    make -C "$tree/addon/fatfs" -j"$jobs"
    make -C "$tree/addon/wlan" -j"$jobs"
    make -C "$tree/addon/wlan/hostap/wpa_supplicant" -f Makefile.circle -j"$jobs"

    touch "$stamp"
}

# build_armstub <target>: armstub8-rpi4.bin for the Pi 4 / CM4 targets
build_armstub() {
    local tree="$TOOLCHAIN_ROOT/circle-$1"

    if [[ ! -f "$tree/boot/armstub/armstub8-rpi4.bin" ]]; then
        log "building armstub8-rpi4.bin for $1"
        make -C "$tree/boot/armstub" armstub8-rpi4.bin
    fi
}

# --- Main -------------------------------------------------------------------

main() {
    local targets=("$@")
    if [[ ${#targets[@]} -eq 0 ]]; then
        read -r -a targets <<< "$ALL_TARGETS"
    fi

    check_host

    # Validate all targets and collect the toolchains they need.
    local triples=""
    for target in "${targets[@]}"; do
        target_config "$target"
        [[ " $triples " == *" $T_TRIPLE "* ]] || triples+=" $T_TRIPLE"
    done

    mkdir -p "$TOOLCHAIN_ROOT"
    for triple in $triples; do
        install_toolchain "$triple"
    done

    install_circle

    for target in "${targets[@]}"; do
        install_tree "$target"
        build_libs "$target"
        if [[ "$target" == rpi4 || "$target" == rpi4-emmc ]]; then
            build_armstub "$target"
        fi
    done

    log "done (targets: ${targets[*]})"
}

main "$@"
