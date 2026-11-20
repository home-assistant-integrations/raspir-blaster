## Why

Builds and tests only run on the developer's Mac today, so a broken commit is noticed late. There is also no defined way to produce a release: version numbers, the firmware's reported version and the downloadable SD card files are all manual. Two GitHub Actions workflows fix this:
- Every change is built and tested automatically.
- Releases get a reproducible branch, a version number and published artifacts.

## What Changes

- **CI workflow** (`.github/workflows/ci.yml`):
  - **Triggers:** every push to any branch except release branches. This includes feature branches, and `main` when a pull request is merged.
  - **Runner:** a GitHub-hosted Apple Silicon macOS runner.
  - **Steps:** builds all four firmware targets, runs the host unit tests and assembles the SD card / CM4 eMMC directories. This also checks every pinned download and checksum.
  - **Caching:** `toolchain/` is cached and keyed on the pinned versions.
  - The kernel images are kept as a short-lived workflow artifact.
- **Release workflow** (`.github/workflows/release.yml`), started manually from `main`:
  1. Computes the next version `yyyy.mm.rm`: UTC year, two-digit month, and a release counter that starts at 0 and resets every month.
  2. Creates the release branch `rel-<version>` from `main` and commits a `VERSION` file to it.
  3. Builds and tests on that branch.
  4. Publishes a GitHub Release with tag `<version>`. The assets are `raspir-blaster-<version>-sdcard.zip`, `raspir-blaster-<version>-cm4-emmc.zip` and `SHA256SUMS.txt`.
  5. If the build fails, the release branch is deleted again, so the version number is not used up.
- **Firmware version:** the version is no longer hard-coded (`0.1.0`).
  - The build takes it from the `VERSION` file, or from `make VERSION=…`; default `dev`.
  - It is embedded in every image: the boot banner and the Home Assistant `sw_version`.
  - A changed version triggers a rebuild of the affected objects.

## Capabilities

### New Capabilities
- `continuous-integration`: when the CI workflow runs, what it checks, the runner, caching, concurrency and the CI artifacts.
- `release-process`: release trigger and permissions, version scheme and numbering, release branch, build on the branch, published assets, failure cleanup.

### Modified Capabilities
- `firmware-build`: new requirement "Firmware version". The firmware version comes from the `VERSION` file or the `VERSION` make variable (default `dev`) and is applied to all targets. Existing requirements are unchanged.

## Non-goals

- **Hardware tests in CI.** The hardware verification change stays manual.
- **Release triggers other than manual dispatch from `main`**, e.g. scheduled or tag-push releases.
- **Release notes beyond GitHub's generated notes** plus a fixed header about which file belongs to which board.
- **Signing artifacts** (GPG, Sigstore). Integrity is provided by `SHA256SUMS.txt` and GitHub's HTTPS download.
- **Linux or Intel macOS runners**, and self-hosted runners.
- **Pull-request-only checks** (`pull_request` events). Pushes to the PR's branch already run CI.
- **Patch releases on existing release branches.** A release branch is built once; fixes go to `main` and into a new release.

## Impact

- **New files:**
  - `.github/workflows/ci.yml`
  - `.github/workflows/release.yml`
  - `scripts/next-version.sh` (version calculation, host-testable)
  - `scripts/package-release.sh` (zips and checksums)
  - `.github/release-notes-header.md`
- **Build changes:**
  - Top-level `Makefile`: `VERSION` handling, generated version header.
  - `src/core/version.h`: uses the generated version, otherwise `dev`.
  - `.gitignore` is unchanged, because the generated header lives under `build/`.
- **Repository settings:**
  - Workflows need `contents: write` for the release.
  - If `main` gets branch protection later, the release workflow only reads it.
  - `rel-*` branches must not be protected, so the cleanup job can delete them.
- **Cost/time:** macOS minutes are free for this public repository. A cold CI run (no cache) takes about 5 minutes, a cached run about 2.
- **Docs:** README (CI badge, releases, version scheme) and `CLAUDE.md` (workflows, version source).
