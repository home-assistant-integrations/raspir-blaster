## Context

The release workflow (`.github/workflows/release.yml`) has four jobs: `prepare` (on `ubuntu-latest`: check `main`, compute the version with `scripts/next-version.sh`, create and push `rel-<version>` with a `VERSION` commit), `build` (macOS), `publish` (GitHub release with header + generated notes) and `cleanup` (on failure, delete branch/tag/release that point to this run's commit). `main` is never written by the workflow, and the README states "never commit `VERSION` on `main`". `main` has no active branch protection; a ruleset named `main` exists but is disabled.

There is no `CHANGELOG.md` yet. Release `2026.11.0` was published on 2026-11-20 with header + generated notes. Release helper scripts are bash, live in `scripts/`, and have tests in `tests/scripts/test-*.sh` that `make test` runs (`tests/Makefile`, target `scripts`).

## Goals / Non-Goals

**Goals:**
- Every release has a dated `CHANGELOG.md` section, created by the workflow and not by hand.
- The workflow never writes to `main` (the maintainer's decision), but forgetting to copy the changelog back is detected and cannot cause duplicate entries.
- The changelog logic is testable without GitHub.

**Non-Goals:**
- See the proposal's "Non-goals": no push to `main`, no generated entries, no content linting, no rewrite of `2026.11.0`'s notes.

## Decisions

### D1 — Changelog commit on the release branch only
The update is made in `prepare` on the local `rel-<version>` branch before it is pushed, in the existing `chore(release): <version>` commit together with `VERSION`.
*Alternative:* commit to `main` first and branch from it. Rejected by the maintainer: it needs write access to `main` from Actions (breaks once the `main` ruleset is enabled), and a failed release would leave a changelog section for a version that does not exist on `main`.
*Consequence:* the failure cleanup stays as it is; deleting `rel-<version>` also discards the changelog update.

### D2 — Copy-back guard instead of automation
After a release, `main`'s `[Unreleased]` still holds the released entries until the maintainer merges a copy-back PR (`git checkout rel-<version> -- CHANGELOG.md`). Releasing again before that would release the same entries twice. `prepare` therefore determines the previous release (highest tag matching `^[0-9]{4}\.[0-9]{2}\.(0|[1-9][0-9]*)$` from `git ls-remote --tags origin`, numeric sort per field) and refuses to run unless `CHANGELOG.md` has a `## [<previous>]` heading and the `[unreleased]` link compares from `<previous>`. The error names the branch to copy from.
*Alternative:* merge `rel-<version>` back. Rejected: it would bring `VERSION` onto `main`.

### D3 — One script, two commands: `scripts/changelog.sh`
- `changelog.sh [-f <file>] release <version> <date> [<previous>]` rewrites the file in place (default `CHANGELOG.md`; `-f` is an option because two optional positional arguments would be ambiguous):
  1. Fails if `## [Unreleased]` or the `[unreleased]: <base>/compare/<from>...HEAD` link is missing, or `<version>` already has a section.
  2. If `<previous>` is given, fails unless `## [<previous>]` exists and `<from>` equals `<previous>` (D2).
  3. Collects the `[Unreleased]` body (up to the next `## [` heading or the link block), drops `###` headings without entries, and fails if no entry is left. An entry is any non-blank line that is not a `###` heading or an HTML comment.
  4. Writes `## [Unreleased]`, a blank line, `## [<version>] - <date>`, the kept body, and the rest of the file; replaces the `[unreleased]` link and inserts `[<version>]: <base>/compare/<from>...<version>` below it.
- `changelog.sh [-f <file>] section <version>` prints the body of `## [<version>] - …` (without the heading), trimmed of leading/trailing blank lines; fails if the section is missing or empty.

Written in bash + `awk` (both present on `ubuntu-latest` and macOS; BSD and GNU `awk` compatible: no `gawk` extensions). Messages go to stderr with the prefix `changelog:`, exit code 1 on any error. The file is written via a temp file and `mv`, so a failure never leaves a half-written changelog.
*Alternative:* Python or a GitHub Action from the marketplace. Rejected: adds a dependency or a third-party action with `contents: write` for ~100 lines of text processing.

### D4 — Release date and previous release are inputs, not computed inside the script
`prepare` passes `date -u +%F` and the previous release. This keeps the script deterministic, so tests do not depend on the clock or a remote.

### D5 — Release notes layout
`publish` runs on `rel-<version>`, so it reads the released section from that branch's `CHANGELOG.md`:
```
<header with <version> filled in>

## What's new in <version>

<changelog.sh section <version>>

<GitHub generated notes, via --generate-notes>
```
The version's `###` category headings sit under the `##` heading, matching the header's `## Downloads` level. `gh release create --notes-file … --generate-notes` already appends generated notes after the file's content.

### D6 — Check order in `prepare`
Compute version → compute previous release → `changelog.sh release` on the checked-out `main` content → only then `git switch -c rel-<version>`, commit, push. Any changelog error therefore stops the run before a branch exists, and `cleanup` has nothing to do (its `version != ''` condition still triggers it, but it only deletes refs pointing to this run's commit, which do not exist).

## Risks / Trade-offs

- [Copy-back PR forgotten] → D2 guard blocks the next release with a precise message; README documents the step.
- [Hand-edited changelog in an unexpected shape, e.g. lowercase heading or missing link] → script fails with a message naming the missing part, before any branch exists; tests cover these cases.
- [`awk` dialect differences between macOS (tests run locally) and Ubuntu (workflow)] → only POSIX `awk` features; `make test` runs on macOS in CI, and the script is also exercised on Ubuntu by every release.
- [Two releases in one day] → both get the same date; versions differ, so sections stay unique.
- [The `[unreleased]` link compares from a tag that was deleted] → GitHub shows a broken compare link; not detected, out of scope.

## Migration Plan

1. Merge this change (adds `CHANGELOG.md` with `[2026.11.0]` and the current `[Unreleased]` entries).
2. The next release uses the new flow. No change to existing branches, tags or releases. Rollback: revert the workflow change; `CHANGELOG.md` can stay.
