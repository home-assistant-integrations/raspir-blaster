#!/usr/bin/env bash
#
#  changelog.sh
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

# Maintains CHANGELOG.md (Keep a Changelog 1.1.0) for releases, see
# openspec/specs/release-process ("Changelog").
#
#   changelog.sh [-f FILE] release VERSION DATE [PREVIOUS]
#       Moves the [Unreleased] entries into "## [VERSION] - DATE" (in place),
#       drops empty category headings, and updates the link references.
#       Fails if [Unreleased] has no entries, VERSION already has a section,
#       or (with PREVIOUS) the file does not contain release PREVIOUS yet.
#
#   changelog.sh [-f FILE] section VERSION
#       Prints the entries of VERSION's section, without its heading.
#
# FILE defaults to CHANGELOG.md. Only POSIX awk features are used, so this
# runs with BSD awk (macOS) and mawk/gawk (Ubuntu).

set -euo pipefail

die() {
    printf 'changelog: %s\n' "$*" >&2
    exit 1
}

usage() {
    die "usage: changelog.sh [-f FILE] release VERSION DATE [PREVIOUS] | [-f FILE] section VERSION"
}

file="CHANGELOG.md"
if [[ "${1:-}" == "-f" ]]; then
    [[ $# -ge 2 ]] || usage
    file="$2"
    shift 2
fi
[[ $# -ge 1 ]] || usage
command="$1"
shift

[[ -f "$file" ]] || die "$file not found"

version_re='^[0-9]{4}\.[0-9]{2}\.(0|[1-9][0-9]*)$'

# Shared awk helpers: error exit and the definition of an entry line.
AWK_LIB='
function fail(msg) {
    print "changelog: " msg > "/dev/stderr"
    failed = 1
    exit 1
}
function is_entry(s) {
    return s !~ /^[ \t]*$/ && s !~ /^### / && s !~ /^[ \t]*<!--.*-->[ \t]*$/
}
function is_section_end(s) {
    return s ~ /^## / || s ~ /^\[[^]]+\]: /
}
'

case "$command" in
release)
    [[ $# -eq 2 || $# -eq 3 ]] || usage
    version="$1"
    date="$2"
    previous="${3:-}"
    [[ "$version" =~ $version_re ]] || die "invalid version \"$version\""
    [[ "$date" =~ ^[0-9]{4}-[0-9]{2}-[0-9]{2}$ ]] || die "invalid date \"$date\" (expected yyyy-mm-dd)"
    [[ -z "$previous" || "$previous" =~ $version_re ]] || die "invalid previous version \"$previous\""

    tmp="$(mktemp "$file.XXXXXX")"
    trap 'rm -f "$tmp"' EXIT
    awk -v version="$version" -v date="$date" -v previous="$previous" -v file="$file" "$AWK_LIB"'
    { line[NR] = $0 }
    END {
        if (failed) exit 1
        n = NR; u = 0; lnk = 0; has_prev = 0
        for (i = 1; i <= n; i++) {
            if (u == 0 && line[i] ~ /^## \[Unreleased\][ \t]*$/) u = i
            if (index(line[i], "## [" version "]") == 1)
                fail("version " version " already has a section in " file)
            if (previous != "" && index(line[i], "## [" previous "]") == 1) has_prev = 1
            if (lnk == 0 && line[i] ~ /^\[[Uu]nreleased\]: /) lnk = i
        }
        if (u == 0) fail("no \"## [Unreleased]\" heading in " file)
        if (lnk == 0) fail("no \"[unreleased]: <repository>/compare/<version>...HEAD\" link in " file)

        url = substr(line[lnk], index(line[lnk], ": ") + 2)
        c = index(url, "/compare/")
        if (c == 0 || url !~ /\.\.\.HEAD$/)
            fail("[unreleased] link \"" url "\" is not \"<repository>/compare/<version>...HEAD\"")
        base = substr(url, 1, c - 1)
        from = substr(url, c + 9, length(url) - c - 8 - 7)

        if (previous != "" && !has_prev)
            fail(file " does not contain release " previous " yet; copy CHANGELOG.md from rel-" previous " to main first")
        if (previous != "" && from != previous)
            fail("[unreleased] link compares from " from ", but the previous release is " previous)

        # Body of [Unreleased]: up to the next "## " heading or link reference.
        e = u + 1
        while (e <= n && !is_section_end(line[e])) e++

        # Keep text before the first category and every category with entries.
        kept = 0; entries = 0; in_cat = 0; g = 0; g_has = 0
        for (i = u + 1; i < e; i++) {
            s = line[i]
            if (s ~ /^### /) {
                if (in_cat && g_has) { for (k = 1; k <= g; k++) body[++kept] = grp[k]; entries++ }
                in_cat = 1; g = 0; g_has = 0
                grp[++g] = s
                continue
            }
            if (in_cat) {
                grp[++g] = s
                if (is_entry(s)) g_has = 1
            } else {
                body[++kept] = s
                if (is_entry(s)) entries++
            }
        }
        if (in_cat && g_has) { for (k = 1; k <= g; k++) body[++kept] = grp[k]; entries++ }
        if (entries == 0) fail("[Unreleased] in " file " has no entries; nothing to release")

        first = 1; while (first <= kept && body[first] ~ /^[ \t]*$/) first++
        last = kept; while (last >= first && body[last] ~ /^[ \t]*$/) last--

        for (i = 1; i <= u; i++) print line[i]
        print ""
        print "## [" version "] - " date
        print ""
        for (i = first; i <= last; i++) print body[i]
        if (e <= n) print ""
        for (i = e; i <= n; i++) {
            if (i == lnk) {
                print "[unreleased]: " base "/compare/" version "...HEAD"
                print "[" version "]: " base "/compare/" from "..." version
            } else {
                print line[i]
            }
        }
    }' "$file" > "$tmp"
    mv "$tmp" "$file"
    trap - EXIT
    ;;

section)
    [[ $# -eq 1 ]] || usage
    version="$1"
    [[ "$version" =~ $version_re ]] || die "invalid version \"$version\""
    awk -v version="$version" -v file="$file" "$AWK_LIB"'
    { line[NR] = $0 }
    END {
        if (failed) exit 1
        n = NR; h = 0
        for (i = 1; i <= n; i++)
            if (index(line[i], "## [" version "]") == 1) { h = i; break }
        if (h == 0) fail("no section for version " version " in " file)
        e = h + 1
        while (e <= n && !is_section_end(line[e])) e++
        first = h + 1; while (first < e && line[first] ~ /^[ \t]*$/) first++
        last = e - 1; while (last >= first && line[last] ~ /^[ \t]*$/) last--
        entries = 0
        for (i = first; i <= last; i++) if (is_entry(line[i])) entries++
        if (entries == 0) fail("section " version " in " file " has no entries")
        for (i = first; i <= last; i++) print line[i]
    }' "$file"
    ;;

*)
    usage
    ;;
esac
