## ADDED Requirements

### Requirement: Firmware version
The firmware version embedded in all kernel images SHALL be taken from, in this order:
1. the make variable `VERSION` (e.g. `make VERSION=2026.11.0`)
2. the file `VERSION` in the repository root, if it exists (only release branches have it)
3. the default `dev`

The version SHALL appear in the boot banner and as `sw_version` in Home Assistant discovery. When the version changes, the next build SHALL rebuild the affected objects, without a `make clean`. The host unit tests SHALL build without a version being set.

#### Scenario: Development build
- **WHEN** `make` runs on a branch without a `VERSION` file
- **THEN** the firmware reports the version `dev`

#### Scenario: Release branch build
- **WHEN** `make` runs on a branch whose `VERSION` file contains `2026.11.0`
- **THEN** all kernel images report the version `2026.11.0`

#### Scenario: Version change triggers rebuild
- **WHEN** `make` has built with version `dev` and then runs with `VERSION=2026.11.0`
- **THEN** the images report `2026.11.0` without a prior `make clean`
