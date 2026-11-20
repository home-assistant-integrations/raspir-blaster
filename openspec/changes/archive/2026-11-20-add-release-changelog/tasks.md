## 1. Changelog file

- [x] 1.1 `CHANGELOG.md`: Keep a Changelog 1.1.0 header, `[Unreleased]` (enclosure, release changelog), `[2026.11.0] - 2026-11-20`, links `[unreleased]` (compare `2026.11.0...HEAD`) and `[2026.11.0]` (release tag, the first release has no predecessor)

## 2. Changelog script

- [x] 2.1 `scripts/changelog.sh [-f <file>] section <version>`: print the section body, fail on missing or empty section (D3)
- [x] 2.2 `scripts/changelog.sh [-f <file>] release <version> <date> [<previous>]`: structure checks, previous-release guard, empty-category removal, empty-`[Unreleased]` check, atomic rewrite with new heading and links (D3)
- [x] 2.3 `tests/scripts/test-changelog.sh`: release happy path (exact expected file), empty categories dropped, empty `[Unreleased]` fails and leaves the file unchanged, missing previous section fails, link not comparing from previous fails, version already released fails, missing `[unreleased]` link fails
- [x] 2.4 Tests for `section`: body printed without heading for a middle and the last section, missing version fails; run `make test` and check the scripts suite passes on macOS

## 3. Release workflow

- [x] 3.1 `prepare`: compute the previous release from `git ls-remote --tags origin` (highest `yyyy.mm.rm`, numeric sort), run `scripts/changelog.sh release "$VERSION" "$(date -u +%F)" "$PREVIOUS"` before `git switch -c`, and add `CHANGELOG.md` to the `chore(release)` commit (D6)
- [x] 3.2 `publish`: build the notes file from the header, `## What's new in <version>` and `scripts/changelog.sh section`; keep `--generate-notes` (D5)
- [x] 3.3 Update the workflow's top comment; lint `release.yml` (YAML parse, `bash -n` on the `run` blocks)
- [x] 3.4 Dry-run the `prepare` changelog logic locally against the real remote (`git ls-remote`) on a scratch copy of the repository, without pushing; check the resulting commit and that an empty `[Unreleased]` stops before the branch is created

## 4. Documentation

- [x] 4.1 README "Making a release": add changelog entries to `[Unreleased]` in each PR, the two preconditions, the copy-back PR after a release, and the new notes layout
- [x] 4.2 `CLAUDE.md`: mention `CHANGELOG.md`, `scripts/changelog.sh`, and that the changelog update is only committed on `rel-*`
