## ADDED Requirements

### Requirement: Changelog
The repository SHALL contain a `CHANGELOG.md` in the [Keep a Changelog 1.1.0](https://keepachangelog.com/en/1.1.0/) format, with an `## [Unreleased]` section followed by one `## [<version>] - <yyyy-mm-dd>` section per release (newest first), and link references at the end: `[unreleased]: <repository>/compare/<latest release>...HEAD` and one `[<version>]` link per release.

Before the release branch is created, the release workflow SHALL update `CHANGELOG.md` for the computed version:
- The entries of `[Unreleased]` become a new section `## [<version>] - <date>` directly below `## [Unreleased]`, where `<date>` is the release date in UTC. Category headings (`### Added`, `### Fixed`, ...) without entries are dropped. `[Unreleased]` is left without entries.
- The `[unreleased]` link changes to `<repository>/compare/<version>...HEAD`, and the link `[<version>]: <repository>/compare/<previous>...<version>` is added below it, where `<previous>` is the version the `[unreleased]` link compared from.

The workflow SHALL fail before creating the release branch, without changing anything in the repository, if:
- `[Unreleased]` contains no entries (only blank lines and category headings), or
- a previous release exists and `CHANGELOG.md` on `main` has no `## [<previous release>]` section or its `[unreleased]` link does not compare from it, where the previous release is the highest existing `yyyy.mm.rm` tag.

The workflow SHALL NOT push to `main`. After a release, the maintainer SHALL bring the released `CHANGELOG.md` back to `main` through a pull request.

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

#### Scenario: Main is not changed
- **WHEN** a release succeeds
- **THEN** `main` has no new commit from the workflow, and its `CHANGELOG.md` is unchanged until the maintainer's copy-back pull request is merged

## MODIFIED Requirements

### Requirement: Release branch
Before building, the release workflow SHALL create the branch `rel-<version>` from the `main` commit the run was started on. On it, the workflow SHALL make one commit `chore(release): <version>`, authored by `github-actions[bot]`, that adds a file `VERSION` containing the version and contains the `CHANGELOG.md` updated for the version (see "Changelog").

#### Scenario: Branch creation
- **WHEN** the version `2026.11.0` has been computed
- **THEN** the branch `rel-2026.11.0` exists in the repository, its head commit adds `VERSION` with content `2026.11.0` and updates `CHANGELOG.md` with the section `## [2026.11.0]`, and its parent is the released `main` commit

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
