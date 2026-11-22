## MODIFIED Requirements

### Requirement: Changelog
The repository SHALL contain a `CHANGELOG.md` in the [Keep a Changelog 1.1.0](https://keepachangelog.com/en/1.1.0/) format, with an `## [Unreleased]` section followed by one `## [<version>] - <yyyy-mm-dd>` section per release (newest first), and link references at the end: `[unreleased]: <repository>/compare/<latest release>...HEAD` and one `[<version>]` link per release.

Before the release branch is created, the release workflow SHALL update `CHANGELOG.md` for the computed version:
- The entries of `[Unreleased]` become a new section `## [<version>] - <date>` directly below `## [Unreleased]`, where `<date>` is the release date in UTC. Category headings (`### Added`, `### Fixed`, ...) without entries are dropped. `[Unreleased]` is left without entries.
- The `[unreleased]` link changes to `<repository>/compare/<version>...HEAD`, and the link `[<version>]: <repository>/compare/<previous>...<version>` is added below it, where `<previous>` is the version the `[unreleased]` link compared from.

The workflow SHALL fail before creating the release branch, without changing anything in the repository, if:
- `[Unreleased]` contains no entries (only blank lines and category headings), or
- a previous release exists and `CHANGELOG.md` on `main` has no `## [<previous release>]` section or its `[unreleased]` link does not compare from it, where the previous release is the highest existing `yyyy.mm.rm` tag.

After a successful release, the workflow SHALL bring the released `CHANGELOG.md` back to `main` itself, as a commit it pushes directly (no pull request):
- It SHALL replace `main`'s `CHANGELOG.md` with the released version's file, except that it SHALL keep `main`'s own `[Unreleased]` entries (whatever was merged to `main` while the release was building) instead of the released file's empty `[Unreleased]`.
- This step SHALL be idempotent: if `main` already has a `## [<version>]` section, it SHALL make no change and SHALL NOT fail.
- If pushing to `main` fails because another commit landed on `main` first, the workflow SHALL retry (re-reading `main`'s current state each time) at least once; it MAY give up after a bounded number of attempts.
- A failure of this step (including exhausting its retries) SHALL NOT delete, modify or otherwise affect the already-published release, tag or release branch. It SHALL be clearly reported (e.g. a failed workflow run) with the manual recovery procedure.

#### Scenario: Unreleased entries become the release section
- **WHEN** `CHANGELOG.md` on `main` has `### Added` with one entry under `[Unreleased]`, the `[unreleased]` link compares from `2026.11.0`, and release `2026.11.1` is made on 2026-11-25 (UTC)
- **THEN** the `CHANGELOG.md` on `rel-2026.11.1` has an empty `## [Unreleased]`, followed by `## [2026.11.1] - 2026-11-25` with that `### Added` entry, the link `[unreleased]: <repository>/compare/2026.11.1...HEAD` and the link `[2026.11.1]: <repository>/compare/2026.11.0...2026.11.1`

#### Scenario: Empty category headings are dropped
- **WHEN** `[Unreleased]` contains `### Added` with entries and `### Fixed` without entries
- **THEN** the new release section contains `### Added` with its entries and no `### Fixed` heading

#### Scenario: Nothing to release
- **WHEN** `[Unreleased]` contains no entries and a release is started
- **THEN** the workflow fails with a message that `[Unreleased]` in `CHANGELOG.md` is empty, and no branch, tag or release is created

#### Scenario: Previous release not copied back
- **WHEN** release `2026.11.1` exists, `CHANGELOG.md` on `main` has no `## [2026.11.1]` section, and another release is started
- **THEN** the workflow fails with a message to copy `CHANGELOG.md` from `rel-2026.11.1` to `main` first, and no branch, tag or release is created

#### Scenario: Changelog synced back automatically
- **WHEN** release `2026.11.3` is published and no other commits landed on `main` while it was building
- **THEN** `main`'s `CHANGELOG.md` gains a `## [2026.11.3]` section matching `rel-2026.11.3`'s, `[Unreleased]` stays empty, and the next release does not fail the "previous release not copied back" check

#### Scenario: Entries merged during the release are kept
- **WHEN** release `2026.11.3` is building and a pull request adding a `[Unreleased]` entry merges to `main` in the meantime
- **THEN** after the sync step, `main`'s `CHANGELOG.md` has that new entry under `[Unreleased]` and the `## [2026.11.3]` section below it, in that order

#### Scenario: Sync does not undo a published release
- **WHEN** the sync step fails after exhausting its retries
- **THEN** the release `2026.11.3`, its tag and its branch `rel-2026.11.3` still exist unchanged, and the workflow run is reported as failed with the manual recovery procedure
