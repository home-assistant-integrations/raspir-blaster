#!/usr/bin/env bash
#
#  test-changelog.sh
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

# Tests for scripts/changelog.sh. Run by "make test".

set -uo pipefail

SCRIPT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)/scripts/changelog.sh"
WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT
failures=0
count=0
REPO=https://github.com/o/r

fail() {
    printf '[FAIL] %s\n' "$*"
    failures=$((failures + 1))
}

# A changelog with released versions 2026.11.0 and 2026.11.1 and the given
# [Unreleased] body (from stdin), written to WORK/CHANGELOG.md, or to
# WORK/<name> if a file name is given.
make_changelog() {
    local out="$WORK/${1:-CHANGELOG.md}"
    {
        printf '# Changelog\n\nIntro.\n\n## [Unreleased]\n'
        cat
        printf '\n## [2026.11.1] - 2026-11-10\n\n### Fixed\n\n- Old fix.\n'
        printf '\n## [2026.11.0] - 2026-11-01\n\n### Added\n\n- First.\n\n'
        printf '[unreleased]: %s/compare/2026.11.1...HEAD\n' "$REPO"
        printf '[2026.11.1]: %s/compare/2026.11.0...2026.11.1\n' "$REPO"
        printf '[2026.11.0]: %s/releases/tag/2026.11.0\n' "$REPO"
    } > "$out"
}

# expect_release_ok <name> <expected file (stdin)> -- <release args...>
expect_release_ok() {
    local name="$1"
    shift
    count=$((count + 1))
    cat > "$WORK/expected.md"
    local err
    if ! err="$("$SCRIPT" -f "$WORK/CHANGELOG.md" release "$@" 2>&1)"; then
        fail "$name: unexpected failure: $err"
        return
    fi
    if ! diff -u "$WORK/expected.md" "$WORK/CHANGELOG.md" > "$WORK/diff.txt"; then
        fail "$name: unexpected result"
        cat "$WORK/diff.txt"
    fi
}

# expect_release_fails <name> <expected message> <release args...>
# The file must stay unchanged.
expect_release_fails() {
    local name="$1" message="$2"
    shift 2
    count=$((count + 1))
    cp "$WORK/CHANGELOG.md" "$WORK/before.md"
    local err
    if err="$("$SCRIPT" -f "$WORK/CHANGELOG.md" release "$@" 2>&1)"; then
        fail "$name: expected failure, succeeded"
    elif [[ "$err" != "changelog: $message" ]]; then
        fail "$name: expected \"changelog: $message\", got \"$err\""
    fi
    if ! cmp -s "$WORK/before.md" "$WORK/CHANGELOG.md"; then
        fail "$name: file was changed"
    fi
    if compgen -G "$WORK/CHANGELOG.md.*" > /dev/null; then
        fail "$name: temporary file left behind"
    fi
}

# expect_section <name> <version> <expected output or "!message">
expect_section() {
    local name="$1" version="$2" expected="$3"
    count=$((count + 1))
    local out
    if out="$("$SCRIPT" -f "$WORK/CHANGELOG.md" section "$version" 2>&1)"; then
        [[ "$out" == "$expected" ]] || fail "$name: expected \"$expected\", got \"$out\""
    else
        [[ "!$out" == "$expected" ]] || fail "$name: expected \"$expected\", got \"$out\""
    fi
}

# expect_backport_ok <name> <expected file (stdin)>
# Runs "backport" of WORK/released.md into WORK/CHANGELOG.md.
expect_backport_ok() {
    local name="$1"
    count=$((count + 1))
    cat > "$WORK/expected.md"
    local err
    if ! err="$("$SCRIPT" -f "$WORK/CHANGELOG.md" backport "$WORK/released.md" 2>&1)"; then
        fail "$name: unexpected failure: $err"
        return
    fi
    if ! diff -u "$WORK/expected.md" "$WORK/CHANGELOG.md" > "$WORK/diff.txt"; then
        fail "$name: unexpected result"
        cat "$WORK/diff.txt"
    fi
}

# --- release ---------------------------------------------------------------

make_changelog <<'EOF'

### Added

- New thing.
  Continued line.

### Fixed

- A bug.

EOF
expect_release_ok "release moves entries and links" 2026.11.2 2026-11-25 2026.11.1 <<EOF
# Changelog

Intro.

## [Unreleased]

## [2026.11.2] - 2026-11-25

### Added

- New thing.
  Continued line.

### Fixed

- A bug.

## [2026.11.1] - 2026-11-10

### Fixed

- Old fix.

## [2026.11.0] - 2026-11-01

### Added

- First.

[unreleased]: $REPO/compare/2026.11.2...HEAD
[2026.11.2]: $REPO/compare/2026.11.1...2026.11.2
[2026.11.1]: $REPO/compare/2026.11.0...2026.11.1
[2026.11.0]: $REPO/releases/tag/2026.11.0
EOF

make_changelog <<'EOF'

### Added

<!-- placeholder -->

### Changed

- Changed thing.

### Fixed

EOF
expect_release_ok "empty categories dropped, no previous given" 2026.12.0 2026-12-01 <<EOF
# Changelog

Intro.

## [Unreleased]

## [2026.12.0] - 2026-12-01

### Changed

- Changed thing.

## [2026.11.1] - 2026-11-10

### Fixed

- Old fix.

## [2026.11.0] - 2026-11-01

### Added

- First.

[unreleased]: $REPO/compare/2026.12.0...HEAD
[2026.12.0]: $REPO/compare/2026.11.1...2026.12.0
[2026.11.1]: $REPO/compare/2026.11.0...2026.11.1
[2026.11.0]: $REPO/releases/tag/2026.11.0
EOF

make_changelog <<'EOF'

### Added

### Fixed

EOF
expect_release_fails "only empty categories" \
    "[Unreleased] in $WORK/CHANGELOG.md has no entries; nothing to release" \
    2026.11.2 2026-11-25 2026.11.1

make_changelog < /dev/null
expect_release_fails "empty [Unreleased]" \
    "[Unreleased] in $WORK/CHANGELOG.md has no entries; nothing to release" \
    2026.11.2 2026-11-25 2026.11.1

make_changelog <<'EOF'

### Added

- New thing.
EOF
expect_release_fails "previous release not copied back" \
    "$WORK/CHANGELOG.md does not contain release 2026.11.2 yet; copy CHANGELOG.md from rel-2026.11.2 to main first" \
    2026.11.3 2026-11-25 2026.11.2
expect_release_fails "version already released" \
    "version 2026.11.1 already has a section in $WORK/CHANGELOG.md" \
    2026.11.1 2026-11-25 2026.11.0
expect_release_fails "link does not compare from previous" \
    "[unreleased] link compares from 2026.11.1, but the previous release is 2026.11.0" \
    2026.11.2 2026-11-25 2026.11.0
expect_release_fails "invalid version" 'invalid version "v2026.11.2"' v2026.11.2 2026-11-25
expect_release_fails "invalid date" 'invalid date "25.11.2026" (expected yyyy-mm-dd)' 2026.11.2 25.11.2026

sed -i.bak '/^\[unreleased\]:/d' "$WORK/CHANGELOG.md" && rm -f "$WORK/CHANGELOG.md.bak"
expect_release_fails "missing [unreleased] link" \
    "no \"[unreleased]: <repository>/compare/<version>...HEAD\" link in $WORK/CHANGELOG.md" \
    2026.11.2 2026-11-25 2026.11.1

make_changelog <<'EOF'

- New thing.
EOF
sed -i.bak 's/^## \[Unreleased\]/## Unreleased/' "$WORK/CHANGELOG.md" && rm -f "$WORK/CHANGELOG.md.bak"
expect_release_fails "missing [Unreleased] heading" \
    "no \"## [Unreleased]\" heading in $WORK/CHANGELOG.md" \
    2026.11.2 2026-11-25 2026.11.1

# --- backport ---------------------------------------------------------------

# released.md: what rel-2026.11.2 ended up with (branched before the new
# entries below existed on main).
make_changelog released.md <<'EOF'

### Added

- Pre-branch thing.
EOF
"$SCRIPT" -f "$WORK/released.md" release 2026.11.2 2026-11-25 2026.11.1 2> /dev/null

# main's current CHANGELOG.md: new entries merged after the branch point.
make_changelog <<'EOF'

### Added

- Post-branch thing.
EOF
expect_backport_ok "backport keeps new entries and inserts the release" <<EOF
# Changelog

Intro.

## [Unreleased]

### Added

- Post-branch thing.

## [2026.11.2] - 2026-11-25

### Added

- Pre-branch thing.

## [2026.11.1] - 2026-11-10

### Fixed

- Old fix.

## [2026.11.0] - 2026-11-01

### Added

- First.

[unreleased]: $REPO/compare/2026.11.2...HEAD
[2026.11.2]: $REPO/compare/2026.11.1...2026.11.2
[2026.11.1]: $REPO/compare/2026.11.0...2026.11.1
[2026.11.0]: $REPO/releases/tag/2026.11.0
EOF

make_changelog < /dev/null
expect_backport_ok "backport with no new entries matches the released file" < "$WORK/released.md"

count=$((count + 1))
err="$("$SCRIPT" -f "$WORK/CHANGELOG.md" backport "$WORK/released.md" 2>&1)"
if [[ "$err" != "changelog: $WORK/CHANGELOG.md already has a section for 2026.11.2; nothing to backport" ]]; then
    fail "backport is idempotent: unexpected message \"$err\""
fi
if ! diff -u "$WORK/released.md" "$WORK/CHANGELOG.md" > "$WORK/diff.txt"; then
    fail "backport is idempotent: file was changed on a second run"
    cat "$WORK/diff.txt"
fi

count=$((count + 1))
err="$("$SCRIPT" -f "$WORK/CHANGELOG.md" backport "$WORK/no-such-file.md" 2>&1)"
if [[ "$err" != "changelog: $WORK/no-such-file.md not found" ]]; then
    fail "backport missing released file: unexpected message \"$err\""
fi

# The real situation: main's [Unreleased] still has every entry that the
# release moved into its dated section (the release only edits the rel-*
# branch), plus whatever was merged to main while the release was building.
# Only the latter may stay under [Unreleased].
make_changelog released.md <<'EOF'

### Added

- Pre-branch thing.
- Pre-branch entry that spans
  two lines.

### Fixed

- Pre-branch fix.
EOF
"$SCRIPT" -f "$WORK/released.md" release 2026.11.2 2026-11-25 2026.11.1 2> /dev/null

make_changelog <<'EOF'

### Added

- Pre-branch thing.
- Post-branch thing.
- Pre-branch entry that spans
  two lines.

### Fixed

- Pre-branch fix.

### Changed

- Post-branch change.
EOF
expect_backport_ok "backport drops the entries main still has from the release" <<EOF
# Changelog

Intro.

## [Unreleased]

### Added

- Post-branch thing.

### Changed

- Post-branch change.

## [2026.11.2] - 2026-11-25

### Added

- Pre-branch thing.
- Pre-branch entry that spans
  two lines.

### Fixed

- Pre-branch fix.

## [2026.11.1] - 2026-11-10

### Fixed

- Old fix.

## [2026.11.0] - 2026-11-01

### Added

- First.

[unreleased]: $REPO/compare/2026.11.2...HEAD
[2026.11.2]: $REPO/compare/2026.11.1...2026.11.2
[2026.11.1]: $REPO/compare/2026.11.0...2026.11.1
[2026.11.0]: $REPO/releases/tag/2026.11.0
EOF

# Nothing merged to main during the release: main's [Unreleased] is exactly
# what was released, so the result is the released file (42dcd75 instead
# copied the whole 2026.11.3 section back under [Unreleased]).
make_changelog <<'EOF'

### Added

- Pre-branch thing.
- Pre-branch entry that spans
  two lines.

### Fixed

- Pre-branch fix.
EOF
expect_backport_ok "backport of an unchanged main matches the released file" < "$WORK/released.md"

# --- section ---------------------------------------------------------------

make_changelog <<'EOF'

- New thing.
EOF
expect_section "middle section" 2026.11.1 "$(printf '### Fixed\n\n- Old fix.')"
expect_section "last section before links" 2026.11.0 "$(printf '### Added\n\n- First.')"
expect_section "missing version" 2026.10.0 "!changelog: no section for version 2026.10.0 in $WORK/CHANGELOG.md"
expect_section "invalid version" "Unreleased" '!changelog: invalid version "Unreleased"'

# Round trip: a released section is what "section" prints.
"$SCRIPT" -f "$WORK/CHANGELOG.md" release 2026.11.2 2026-11-25 2026.11.1 2> /dev/null
expect_section "section after release" 2026.11.2 "- New thing."

printf '%d changelog tests, %d failed\n' "$count" "$failures"
[[ $failures -eq 0 ]]
