# release-process Specification

## Purpose
How firmware releases are made: the manual release workflow on main, the yyyy.mm.rm version scheme, release branches, published assets and cleanup on failure.
## Requirements
### Requirement: Release trigger
A release SHALL be started manually through the release workflow (`workflow_dispatch`) by a user with write access to the repository. The workflow SHALL only release from `main`. It SHALL refuse to run when started on any other branch.

#### Scenario: Start from main
- **WHEN** a maintainer starts the release workflow on `main`
- **THEN** a release is created from the current head of `main`

#### Scenario: Start from another branch
- **WHEN** the release workflow is started on a feature branch
- **THEN** it fails immediately with the message that releases are only made from `main`, and no branch, tag or release is created

### Requirement: Version scheme
Release versions SHALL have the form `yyyy.mm.rm`:
- `yyyy` is the four-digit year and `mm` the two-digit month (`01`–`12`) of the release date in UTC.
- `rm` is a release counter without leading zeros. It is `0` for the first release of a month and one higher than the highest existing release of the same month otherwise.

Existing releases are determined from the `rel-yyyy.mm.*` branches and `yyyy.mm.*` tags in the repository.

#### Scenario: First release of a month
- **WHEN** the first release in November 2026 is made
- **THEN** its version is `2026.11.0`

#### Scenario: Further release in the same month
- **WHEN** `rel-2026.11.0` and `rel-2026.11.1` exist and another release is made in November 2026
- **THEN** its version is `2026.11.2`

#### Scenario: New month
- **WHEN** releases up to `2026.11.4` exist and a release is made on 1 December 2026 (UTC)
- **THEN** its version is `2026.12.0`

#### Scenario: Gap in the numbering
- **WHEN** only `rel-2026.11.0` and `rel-2026.11.3` exist
- **THEN** the next version is `2026.11.4`

### Requirement: Serialized releases
At most one release workflow run SHALL be active at a time. A second run SHALL wait until the first has finished, so that two runs never compute the same version.

#### Scenario: Two releases started at once
- **WHEN** two release runs are started within seconds
- **THEN** they run one after the other and produce two different versions

### Requirement: Release branch
Before building, the release workflow SHALL create the branch `rel-<version>` from the `main` commit the run was started on. On it, the workflow SHALL make one commit `chore(release): <version>`, authored by `github-actions[bot]`, that adds a file `VERSION` containing the version and contains the `CHANGELOG.md` updated for the version (see "Changelog").

#### Scenario: Branch creation
- **WHEN** the version `2026.11.0` has been computed
- **THEN** the branch `rel-2026.11.0` exists in the repository, its head commit adds `VERSION` with content `2026.11.0` and updates `CHANGELOG.md` with the section `## [2026.11.0]`, and its parent is the released `main` commit

### Requirement: Build on the release branch
The firmware SHALL be built and tested from the release branch `rel-<version>`, with the same steps as in CI (`make`, `make test`, `make sdcard`), and with the firmware version taken from the branch's `VERSION` file.

#### Scenario: Version embedded in the firmware
- **WHEN** release `2026.11.0` is built
- **THEN** every kernel image contains the version `2026.11.0`, which is shown in the boot banner and published as `sw_version` in Home Assistant discovery

### Requirement: Published release
After a successful build, the workflow SHALL publish a GitHub Release with the tag `<version>` pointing to the head of `rel-<version>`, titled `raspir-blaster <version>`. It SHALL have these assets:
- `raspir-blaster-<version>-sdcard.zip`: the contents of `build/sdcard/`
- `raspir-blaster-<version>-cm4-emmc.zip`: the contents of `build/cm4-emmc/`
- `SHA256SUMS.txt`: SHA-256 checksums of both zip files

The release notes SHALL consist of, in this order:
1. a fixed header that explains which zip file belongs to which board,
2. the entries of the version's section in `CHANGELOG.md` on `rel-<version>`, under a heading naming the version,
3. GitHub's generated notes.

#### Scenario: Successful release
- **WHEN** the build of `rel-2026.11.0` succeeds
- **THEN** the release `2026.11.0` is listed on the repository's Releases page with the three assets, and `shasum -a 256 -c SHA256SUMS.txt` succeeds for the downloaded zip files

#### Scenario: Zip contents
- **WHEN** `raspir-blaster-2026.11.0-sdcard.zip` is extracted
- **THEN** it contains the files of the universal SD card at the top level (e.g. `config.txt`, `kernel.img`, `firmware/`), ready to copy onto a FAT32 card

#### Scenario: Changelog in the release notes
- **WHEN** release `2026.11.1` is published and its `CHANGELOG.md` section has `### Fixed` with one entry
- **THEN** the release notes show the download header, then that `### Fixed` entry under a heading naming `2026.11.1`, then GitHub's generated list of changes

### Requirement: Failure cleanup
If any step after creating the release branch fails, the workflow SHALL delete the branch `rel-<version>`, and SHALL NOT create the tag or the release. The version number is then free for the next run.

#### Scenario: Build failure
- **WHEN** the build on `rel-2026.11.2` fails
- **THEN** the branch `rel-2026.11.2` is deleted, no tag or release `2026.11.2` exists, and the next release run computes `2026.11.2` again

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

