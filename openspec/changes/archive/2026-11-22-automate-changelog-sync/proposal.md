## Why

The release workflow refuses to start a release until `CHANGELOG.md` on `main` contains the previous release's section, which only gets there if a maintainer remembers to copy it back with a pull request "before the next release." This has already been forgotten twice (releases `2026.11.1` and `2026.11.2`), both times only noticed when the *next* release failed, weeks later, with "`CHANGELOG.md` does not contain release `<previous>` yet." A step a human must remember, with no reminder and a delayed failure, is not a durable process; the copy-back belongs in the workflow.

## What Changes

- **`scripts/changelog.sh backport RELEASED_FILE`:** a new subcommand. Given a "release"-processed changelog (e.g. `rel-<version>`'s `CHANGELOG.md`, which has the new version's dated section and an empty `[Unreleased]`), replaces the current file with it but keeps the current file's own `[Unreleased]` entries (whatever was merged to `main` since the release branched) instead of the released file's empty one. A no-op, exit 0, if the current file already has a section for the released file's newest version.
- **Release workflow, new job `sync-changelog`:** runs after `publish` succeeds. Checks out `main`, runs `backport` against `rel-<version>`'s `CHANGELOG.md`, commits (`docs(changelog): bring back <version>`) and pushes straight to `main` — no pull request. Retries up to 5 times (re-reading `main` each time) if the push races with another commit to `main`. Not in `cleanup`'s `needs`, so if it ultimately fails, the already-published release is left alone; the workflow run shows the failure and the step summary names the manual fallback.
- **`README.md` / `CLAUDE.md`:** document the automated step; keep the manual copy-back as the documented fallback for when `sync-changelog` itself fails.

## Capabilities

### New Capabilities
<!-- none -->

### Modified Capabilities
- `release-process`: the "Changelog" requirement changes from "the maintainer SHALL bring the released changelog back with a pull request" to "the workflow SHALL push it back to `main` itself after publishing, keeping entries merged to `main` meanwhile, idempotently and with retry on a push race"; the manual pull request becomes a documented fallback, not the normal path.

## Non-goals

- Changing the version scheme, the release branch contents, the build, the published assets, or the failure cleanup for `prepare`/`build`/`publish`.
- Branch protection or required reviews on `main` (none exist today; this still pushes directly, as `VERSION`-bearing commits on `rel-*` already do).
- Making `sync-changelog`'s failure retroactively delete the published release — it must not, since the release itself already succeeded.
- A GitHub Actions auto-merge PR flow (`allow_auto_merge` is off at the repository level, and a PR can still be forgotten exactly like today).

## Impact

- `scripts/changelog.sh` (new `backport` subcommand) and `tests/scripts/test-changelog.sh` (picked up by `make test`).
- `.github/workflows/release.yml` (new `sync-changelog` job).
- `README.md` ("Making a release") and `CLAUDE.md` (release workflow notes).
- No firmware or `src/` changes.
