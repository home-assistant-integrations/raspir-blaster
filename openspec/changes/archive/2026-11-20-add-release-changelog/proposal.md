## Why

Releases have no human-readable record of what changed: the GitHub release notes are only the generated list of pull requests. A `CHANGELOG.md` in the [Keep a Changelog 1.1.0](https://keepachangelog.com/en/1.1.0/) format fixes this, but only if every release turns the `[Unreleased]` entries into a dated version section. Doing that by hand is easy to forget and easy to get wrong (date, version, compare links), so the release workflow does it.

## What Changes

- **`CHANGELOG.md`** in the repository root, in Keep a Changelog 1.1.0 format, with the history up to release `2026.11.0` and an `[Unreleased]` section. Contributors add entries to `[Unreleased]` in their pull requests.
- **Release workflow, `prepare` job:** before the release branch is pushed, the workflow updates `CHANGELOG.md`:
  - The `[Unreleased]` entries become a section `## [<version>] - <yyyy-mm-dd>` (release date in UTC), and `[Unreleased]` is left empty.
  - The `[unreleased]` compare link moves to `<version>...HEAD`, and a `[<version>]` link to `<previous>...<version>` is added.
- **Release branch commit:** the updated `CHANGELOG.md` is committed on `rel-<version>` together with `VERSION`, in the same `chore(release): <version>` commit. `main` is not touched by the workflow.
- **New release preconditions:** the release fails before any branch is created if:
  - `[Unreleased]` has no entries, or
  - `CHANGELOG.md` on `main` does not contain the previous release yet (its changelog was not copied back to `main`).
- **Release notes:** the download header, then the version's `CHANGELOG.md` section, then GitHub's generated notes.
- **Copy-back after a release:** the maintainer brings the released `CHANGELOG.md` back to `main` with a pull request (`git checkout rel-<version> -- CHANGELOG.md`). The README describes this step.
- **`scripts/changelog.sh`:** the changelog logic (release a version, print a version's section), with shell tests in `tests/scripts/` that `make test` runs.

## Capabilities

### New Capabilities
<!-- none -->

### Modified Capabilities
- `release-process`: new requirement for the changelog (format, release update, preconditions, copy-back); the release branch commit also contains the updated `CHANGELOG.md`; the release notes contain the version's changelog section.

## Non-goals

- Pushing to `main` from the workflow, or opening the copy-back pull request automatically.
- Generating changelog entries from commit messages or pull request titles.
- Validating the content of entries (categories, wording) beyond "`[Unreleased]` is not empty".
- Changing the version scheme, the build, the assets or the failure cleanup.
- Rewriting the notes of the existing release `2026.11.0`.

## Impact

- `.github/workflows/release.yml` (`prepare` and `publish` jobs).
- New `scripts/changelog.sh` and `tests/scripts/test-changelog.sh` (picked up by the existing `make test` loop).
- New `CHANGELOG.md`.
- `README.md` ("Making a release": changelog entries, preconditions, copy-back) and `CLAUDE.md` (release workflow notes).
- No firmware or `src/` changes.
