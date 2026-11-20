## ADDED Requirements

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
Before building, the release workflow SHALL create the branch `rel-<version>` from the `main` commit the run was started on. On it, the workflow SHALL commit a file `VERSION` containing the version, authored by `github-actions[bot]`.

#### Scenario: Branch creation
- **WHEN** the version `2026.11.0` has been computed
- **THEN** the branch `rel-2026.11.0` exists in the repository, its head commit adds `VERSION` with content `2026.11.0`, and its parent is the released `main` commit

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

The release notes SHALL start with a fixed header that explains which zip file belongs to which board, followed by GitHub's generated notes.

#### Scenario: Successful release
- **WHEN** the build of `rel-2026.11.0` succeeds
- **THEN** the release `2026.11.0` is listed on the repository's Releases page with the three assets, and `shasum -a 256 -c SHA256SUMS.txt` succeeds for the downloaded zip files

#### Scenario: Zip contents
- **WHEN** `raspir-blaster-2026.11.0-sdcard.zip` is extracted
- **THEN** it contains the files of the universal SD card at the top level (e.g. `config.txt`, `kernel.img`, `firmware/`), ready to copy onto a FAT32 card

### Requirement: Failure cleanup
If any step after creating the release branch fails, the workflow SHALL delete the branch `rel-<version>`, and SHALL NOT create the tag or the release. The version number is then free for the next run.

#### Scenario: Build failure
- **WHEN** the build on `rel-2026.11.2` fails
- **THEN** the branch `rel-2026.11.2` is deleted, no tag or release `2026.11.2` exists, and the next release run computes `2026.11.2` again
