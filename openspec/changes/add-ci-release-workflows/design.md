## Context

The repository is public on GitHub (`home-assistant-integrations/raspir-blaster`, default branch `main`). The build only runs on macOS with Apple Silicon (`scripts/bootstrap.sh` enforces this, because the pinned ARM toolchains exist only as `darwin-arm64` builds). GitHub provides arm64 macOS runners, which are free for public repositories.

A cold build (empty `toolchain/`) downloads about 236 MB and builds Circle four times; locally this takes about 1 minute after the downloads. The extracted toolchains take 1.45 GB, while the Circle trees with built libraries take about 375 MB (uncompressed).

The firmware version is currently the constant `"0.1.0"` in `src/core/version.h`.

## Goals / Non-Goals

**Goals:**
- Every push (except to release branches) is built and tested on the same platform as local development.
- Releases need one click and are reproducible: a branch per release, the version in the firmware, verifiable assets.
- No duplicated build logic between local builds, CI and releases.

**Non-Goals:**
- Hardware tests, signing, scheduled or tag-triggered releases, patch releases on release branches, other runner platforms (see proposal).

## Decisions

### D1 — Runner `macos-15` (arm64)
Both workflows build on `runs-on: macos-15` (GitHub-hosted, Apple Silicon). Jobs that only call `git` and `gh` (version calculation, branch creation, publishing, cleanup) use `ubuntu-latest`, which is faster to start.
*Alternative:* Docker or Linux runners with Linux toolchains. Rejected because `bootstrap.sh` and the pinned checksums are macOS-only, and two host platforms would double the maintenance.

### D2 — One composite action for build and test
`.github/actions/build-and-test/action.yml` contains the steps shared by CI and release:
1. Restore/save the cache (D3).
2. Run `make 2>&1 | tee build/build.log`.
3. Fail on compiler warnings: `grep -E '^[^ ]+:[0-9]+(:[0-9]+)?: warning:' build/build.log`. This matches only compiler diagnostics, not git's "warning: refs/tags/… is not a commit" notice.
4. Run `make test`, then `make sdcard`.

`ci.yml` and the build job of `release.yml` call it, so both always run the same checks.
*Alternative:* copy the steps into both workflows. Rejected because the two would drift apart.

### D3 — Cache downloads and Circle trees, re-extract toolchains
`actions/cache@v4` with:
```yaml
path: |
  toolchain
  !toolchain/arm-gnu-toolchain-*
key: toolchain-macos-arm64-${{ hashFiles('scripts/versions.env', 'scripts/bootstrap.sh', 'scripts/firmware-files.txt') }}
```
The extracted toolchains (1.45 GB) are excluded. On a cache hit, `bootstrap.sh` finds the stamp file but no `gcc` binary, so it re-extracts the cached archive (about 20 s; the SHA-256 is checked again). The Circle trees, their library stamps and the firmware downloads come from the cache. The Circle `Config.mk` files contain absolute toolchain paths, but the runner workspace path (`/Users/runner/work/raspir-blaster/raspir-blaster`) is the same in every run.

There are no `restore-keys`: a partially matching cache could contain Circle trees built with other pins, so a pin change always starts clean.
*Alternative:* cache all of `toolchain/` (about 1.9 GB). Rejected because saving and restoring 1.9 GB takes longer than re-extracting.

### D4 — CI triggers and concurrency
```yaml
on:
  push:
    branches-ignore: ['rel-*']
  workflow_dispatch:
concurrency:
  group: ci-${{ github.ref }}
  cancel-in-progress: true
```
A merged pull request produces a push to `main` and therefore a CI run; no `pull_request` trigger is needed. Because only a branch filter is given, tag pushes do not trigger CI. Pushes made with `GITHUB_TOKEN` (the release workflow's branch) do not start workflows anyway, and `branches-ignore` makes that explicit. Permissions: `contents: read`.

### D5 — Version calculation in a testable script
`scripts/next-version.sh` reads ref names (output of `git ls-remote origin 'refs/heads/rel-*' 'refs/tags/*'`) from stdin and prints the next version:
- The month comes from `date -u +%Y.%m`, or from the environment variable `RELEASE_MONTH` (format `yyyy.mm`) for tests.
- It collects all `rm` values from `refs/heads/rel-<month>.<rm>` and `refs/tags/<month>.<rm>` that match `^[0-9]+$`, and prints `<month>.<max+1>`, or `<month>.0` if there is none.

Host tests in `tests/scripts/test-next-version.sh` cover the scenarios of the spec (first of month, increment, new month, gap, unrelated refs). The script is run by `make test` (the tests Makefile gets a `scripts` step).

### D6 — Version plumbing into the firmware
- **Top-level `Makefile`:** `VERSION ?= $(shell cat VERSION 2>/dev/null)`, with `dev` if empty. The value must match `^[0-9A-Za-z._-]+$`; otherwise the build stops, because the version ends up in a C string and in JSON.
- **Generated header:** before the firmware loop, the Makefile writes `build/generated/firmware_version.h` (`#define RASPIR_FIRMWARE_VERSION "<v>"`) through a temporary file. It replaces the header only if the content differs (`cmp -s`), so an unchanged version does not trigger a rebuild.
- **Firmware Makefile:** `src/fw/Makefile` adds `-I $(SRCROOT)/build/generated` to `EXTRAINCLUDE`. Circle's dependency files (`.d`) then record the header, and a changed version rebuilds exactly the objects that include `version.h`.
- **`src/core/version.h`:** `#if __has_include("firmware_version.h")` includes it, otherwise it defines `"dev"`. The host tests have no generated directory on their include path and therefore build with `dev`.

- **Timestamp-independent rebuild:** macOS (locally and on the runners) ships GNU make 3.81, which compares timestamps with one-second resolution. A version change in the same second as the previous build would therefore not relink. So when the header changes, the Makefile also deletes the objects whose `.d` files list `firmware_version.h`, plus the linked images, instead of relying on timestamps (found during implementation).

*Alternative:* `-DRASPIR_FIRMWARE_VERSION=…` on the compiler command line. Rejected because make does not track flags, so a changed version would need a `make clean`.

### D7 — Release workflow jobs
```yaml
on: workflow_dispatch
concurrency: { group: release, cancel-in-progress: false }
permissions: { contents: write }
```
| Job | Runner | Does |
| --- | --- | --- |
| `prepare` | ubuntu | Fails unless `github.ref == 'refs/heads/main'`. Checks out `main` at `github.sha` and computes the version with `next-version.sh`. Creates `rel-<v>`, commits `VERSION` as `github-actions[bot]` and pushes. Output: `version`. |
| `build` | macos-15 | Checks out `rel-<v>`, runs the composite action from D2, then `scripts/package-release.sh <v>` writes `build/release/*.zip` and `SHA256SUMS.txt`. Uploads them as a workflow artifact `release-<v>` (retention 1 day). |
| `publish` | ubuntu | Downloads the artifact and runs `gh release create <v> --target rel-<v> --title "raspir-blaster <v>" --notes-file .github/release-notes-header.md --generate-notes` with the three assets. |
| `cleanup` | ubuntu | `if: failure() && needs.prepare.outputs.version != ''`. Deletes the release and tag if they exist (`gh release delete <v> --cleanup-tag --yes`), then deletes `rel-<v>` (`git push origin --delete`). |

`--generate-notes` appends GitHub's generated notes to the header file's text. The tag is created by `gh release create` on the head of `rel-<v>`, which is the `VERSION` commit.

### D8 — Packaging
`scripts/package-release.sh <version>` (also runnable locally after `make sdcard`):
```
cd build/sdcard   && zip -r -X -q ../release/raspir-blaster-<v>-sdcard.zip .
cd build/cm4-emmc && zip -r -X -q ../release/raspir-blaster-<v>-cm4-emmc.zip .
cd build/release  && shasum -a 256 *.zip > SHA256SUMS.txt
```
The files sit at the top level of each zip, so the zip's contents can be copied directly onto the card. `-X` leaves out macOS extended attributes. The script refuses to run if either directory is missing or if `VERSION` and the version argument disagree. This prevents packaging a build with the wrong version.

### D9 — Pinning third-party actions
Official actions are referenced by major version (`actions/checkout@v4`, `actions/cache@v4`, `actions/upload-artifact@v4`, `actions/download-artifact@v4`); `gh` is preinstalled on the runners. No third-party marketplace actions are used.
*Alternative:* pin by commit SHA. This can be done later together with Dependabot; it is not needed for official `actions/*`.

## Risks / Trade-offs

- **[`macos-15` image changes]** (Xcode/clang updates) → The firmware uses the pinned ARM toolchains, not Xcode. Only the host tests use Xcode's `clang++`, and their `-Werror` could break on a new clang. That is acceptable: such a failure is real and visible.
- **[The cache is evicted or exceeds 10 GB]** → The cache entry is about 0.5 GB. A miss only costs a cold build of about 5 minutes.
- **[A release branch is left behind when the cleanup job itself fails]** (e.g. a GitHub outage) → The next run takes the next number, which is acceptable because the version scheme allows gaps. The README explains how to delete a stray branch by hand.
- **[Two maintainers release in the same month at the same time]** → The `release` concurrency group runs them one after the other, and `next-version.sh` sees the first run's branch.
- **[Month boundary during a run]** → The version is computed once in `prepare`; later jobs use its output, so a run that crosses midnight UTC keeps its version.
- **[Branch protection or rulesets on `rel-*`]** → They would block the push or the deletion. Documented: `rel-*` must stay unprotected, or must allow `github-actions[bot]`.
- **[GitHub Actions disabled or read-only workflow permissions]** → The workflow sets `permissions: contents: write` explicitly. The repository setting "Workflow permissions" must not restrict this; documented in the README.

## Migration Plan

1. Merge the change to `main`. CI runs right away for that merge.
2. Check the CI run, especially the cache entry's size and the runtimes of cold and cached builds.
3. Start the first release with "Run workflow" on `main` and check the branch, the tag, the assets and `SHA256SUMS.txt`.
4. Rollback: delete or disable the workflow files. Release branches and releases that already exist stay.

## Open Questions

- Should the release workflow also update a `CHANGELOG.md` on `main`? Deferred: GitHub's generated notes are enough for now.
