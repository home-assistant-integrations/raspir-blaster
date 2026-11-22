## 1. Changelog script

- [x] 1.1 `scripts/changelog.sh [-f FILE] backport RELEASED_FILE`: locate `[Unreleased]`/newest version heading in `RELEASED_FILE`, idempotency check against `FILE`, splice `FILE`'s own `[Unreleased]` body into `RELEASED_FILE` (D2)
- [x] 1.2 `tests/scripts/test-changelog.sh`: new entries kept and release spliced in; no new entries matches the released file exactly; idempotent re-run (message, file unchanged); missing `RELEASED_FILE` fails; run `make test`

## 2. Release workflow

- [x] 2.1 New job `sync-changelog` (`needs: [prepare, build, publish]`): checkout `main`, fetch `rel-<version>`'s `CHANGELOG.md`, run `backport`, commit, push with retry-on-race (D1, D3)
- [x] 2.2 Confirm `cleanup`'s `needs` is unchanged (`[prepare, build, publish]`), so `sync-changelog` failing cannot trigger it (D4)
- [x] 2.3 Failure path prints `::error::` with the exact manual fallback command (D5)
- [x] 2.4 Lint `release.yml` (YAML parse, `bash -n` on every `run:` block) and update the workflow's top comment

## 3. Documentation

- [x] 3.1 README "Making a release": describe the automated push-back as the normal path; keep the manual pull-request recipe as the fallback for when it fails; repository settings note that `main` must stay without branch protection
- [x] 3.2 `CLAUDE.md`: release workflow bullet describes `sync-changelog` and the `backport` subcommand
