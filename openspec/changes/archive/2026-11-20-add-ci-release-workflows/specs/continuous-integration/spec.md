## ADDED Requirements

### Requirement: CI triggers
The CI workflow SHALL run on every push to any branch of the repository except release branches (`rel-*`). This includes pushes to feature branches and the push to `main` that results from merging a pull request. It SHALL also be startable manually.

#### Scenario: Push to a feature branch
- **WHEN** a commit is pushed to a feature branch (e.g. `github-actions`)
- **THEN** the CI workflow runs for that commit

#### Scenario: Pull request merged
- **WHEN** a pull request is merged into `main`
- **THEN** the CI workflow runs for the resulting commit on `main`

#### Scenario: Release branch
- **WHEN** a `rel-*` branch is created or updated
- **THEN** the CI workflow does not run (the release workflow builds it)

### Requirement: CI build and test
The CI workflow SHALL run on a GitHub-hosted Apple Silicon macOS runner. For the pushed commit it SHALL:
- build the firmware for all targets (`make`)
- run the host unit tests (`make test`)
- assemble the boot media directories (`make sdcard`), which verifies all pinned downloads and checksums

The workflow SHALL fail if any of these steps fails or if the build log contains a compiler warning.

#### Scenario: Successful run
- **WHEN** the commit builds and all tests pass
- **THEN** the workflow succeeds and the commit shows a green check on GitHub

#### Scenario: Failing unit test
- **WHEN** a unit test fails
- **THEN** the workflow fails, and the failing test is visible in the job log

#### Scenario: Compiler warning
- **WHEN** a firmware source file produces a compiler warning on any target
- **THEN** the workflow fails and names the warning

### Requirement: CI caching
The CI workflow SHALL cache the `toolchain/` directory (toolchains, Circle trees with built libraries, downloads). The cache key SHALL depend on the pinned versions and on the scripts that create `toolchain/`, so that changing a pin invalidates the cache.

#### Scenario: Unchanged pins
- **WHEN** CI runs a second time with unchanged `scripts/versions.env`, `scripts/bootstrap.sh` and `scripts/firmware-files.txt`
- **THEN** the toolchain cache is restored, and nothing is downloaded and no Circle library is rebuilt

#### Scenario: Changed pin
- **WHEN** a commit changes `scripts/versions.env`
- **THEN** the cache is not restored for that key, and the toolchain and Circle are built from scratch

### Requirement: CI concurrency
For the same branch, a newer CI run SHALL cancel a run that is still in progress. Runs on different branches SHALL NOT affect each other.

#### Scenario: Two quick pushes
- **WHEN** two commits are pushed to the same feature branch within a minute
- **THEN** the run of the first commit is cancelled and the run of the second completes

### Requirement: CI artifacts
A successful CI run SHALL upload the kernel images of all targets (`build/images/`) as a workflow artifact named `firmware-images-<short sha>`, kept for 7 days.

#### Scenario: Download a CI build
- **WHEN** a CI run succeeds
- **THEN** the run's summary page offers the artifact with the four kernel images for download for 7 days
