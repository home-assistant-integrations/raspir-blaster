## 1. Firmware version plumbing

- [x] 1.1 `src/core/version.h`: include `firmware_version.h` via `__has_include`, otherwise define `RASPIR_FIRMWARE_VERSION` as `"dev"`; host tests still build and pass
- [x] 1.2 Top-level `Makefile`: `VERSION` from the variable, the `VERSION` file, or `dev`; reject values outside `[0-9A-Za-z._-]`; write `build/generated/firmware_version.h` only when the content changes
- [x] 1.3 `src/fw/Makefile`: add `build/generated` to `EXTRAINCLUDE`; check that `make` → `make VERSION=2026.11.0` rebuilds only the affected objects and that the images contain the new version string

## 2. Version calculation

- [x] 2.1 `scripts/next-version.sh`: read refs from stdin, use `RELEASE_MONTH` or `date -u +%Y.%m`, and print the next `yyyy.mm.rm`
- [x] 2.2 `tests/scripts/test-next-version.sh` covering first of month, increment, new month, gap, tags and unrelated refs; run it from `make test`

## 3. Packaging

- [x] 3.1 `scripts/package-release.sh <version>`: check that the build directories exist and the version matches; write `build/release/raspir-blaster-<v>-sdcard.zip`, `-cm4-emmc.zip` and `SHA256SUMS.txt`
- [x] 3.2 Run it locally after `make VERSION=2026.11.0 sdcard`; check the zip layout (files at the top level) and `shasum -c`
- [x] 3.3 Add `.github/release-notes-header.md` (which zip for which board, how to verify the checksums, link to the README setup sections)

## 4. Shared build action and CI workflow

- [x] 4.1 `.github/actions/build-and-test/action.yml`: cache (path, exclusion, key as in design D3), `make` with log, compiler-warning check, `make test`, `make sdcard`
- [x] 4.2 `.github/workflows/ci.yml`: triggers (push except `rel-*`, `workflow_dispatch`), concurrency, `contents: read`, `macos-15`, checkout, composite action, upload `build/images/` as `firmware-images-<short sha>` (7 days)
- [x] 4.3 Lint both files with `actionlint` (if available) or a YAML parser; check the warning regex against a real build log (no match) and against a log with an injected warning (match)

## 5. Release workflow

- [x] 5.1 `.github/workflows/release.yml`: trigger, concurrency, permissions; `prepare` job (main-only check, `next-version.sh`, create `rel-<v>` with the `VERSION` commit, output `version`)
- [x] 5.2 `build` job: checkout `rel-<v>`, composite action, `package-release.sh`, upload artifact `release-<v>`
- [x] 5.3 `publish` job: download artifact, `gh release create` with tag, target, title, notes header, generated notes and three assets
- [x] 5.4 `cleanup` job: on failure, delete the release/tag if present and delete `rel-<v>`
- [x] 5.5 Lint `release.yml`; dry-run the shell logic of `prepare` locally against the real remote (`git ls-remote`) without pushing

## 6. Documentation and rollout

- [x] 6.1 README: CI badge, "Releases" section (version scheme, downloading, checksum verification, starting a release, repository settings, removing a stray `rel-*` branch)
- [x] 6.2 `CLAUDE.md`: workflows, composite action, version source (`VERSION` file / `make VERSION=`), never commit `VERSION` on `main`
- [x] 6.3 Push the branch and confirm that the CI workflow runs green on GitHub (cold cache), then rerun to confirm the cached run
  - Result (2026-11-20): CI green on `github-actions` (run 44756602726, 3m13s) and on `main` after PR #2 (run 44756681604, 3m06s), both cold ("Cache not found", then "Cache saved"). Cached run confirmed by the Release workflow, which uses the same composite action and key: "Cache hit for: toolchain-macos-arm64-e15ca5be…" (run 44757290987). Note: GitHub caches are branch-scoped; a feature branch's first run reads `main`'s cache, but `main` never reads a feature branch's.
- [x] 6.4 After merging to `main`: run the first release and check the branch, tag, assets and `SHA256SUMS.txt`; record the result in this task
  - Result (2026-11-20): Release run 44757290987 succeeded in 1m23s. Branch `rel-2026.11.0` = `0714afd chore(release): 2026.11.0` on top of `8a5f7af`, `VERSION` = `2026.11.0`; tag `2026.11.0` points to the same commit; release "raspir-blaster 2026.11.0" marked Latest with `raspir-blaster-2026.11.0-sdcard.zip`, `raspir-blaster-2026.11.0-cm4-emmc.zip` and `SHA256SUMS.txt`. Downloaded assets pass `shasum -a 256 -c`; the SD card zip contains `config.txt`, `kernel.img`, `kernel8.img` and `kernel8-rpi4.img`.
