## Context

`release-process` (D2 in `add-release-changelog`'s design) deliberately chose a guard over automation: `prepare` refuses to start a release unless `main`'s `CHANGELOG.md` already has the previous release's section, and the maintainer brings it back by hand with a pull request. The stated reason was that pushing to `main` from Actions "breaks once the `main` ruleset is enabled" — but no such ruleset exists today (`gh api repos/.../branches/main/protection` returns 404), and the guard has not prevented the mistake, only delayed its discovery: it failed releases `2026.11.1`'s and `2026.11.2`'s *successor*, not the release where the step was actually missed. A manual step with no prompt and a delayed failure is not reliable.

## Goals / Non-Goals

**Goals:**
- The changelog comes back to `main` without a maintainer needing to remember anything, for the common case (no branch protection on `main`, which is today's state).
- A failure to sync never undoes an already-published release.
- The sync is idempotent and safe to retry, so a transient failure (a race with another push) can self-heal without manual intervention.

**Non-Goals:** see the proposal's "Non-goals".

## Decisions

### D1 — Direct push to `main`, not a pull request
`sync-changelog` commits `CHANGELOG.md` on a fresh checkout of `main` and runs `git push origin HEAD:main`.
*Alternative:* open a pull request (optionally with auto-merge). Rejected: `allow_auto_merge` is off at the repository level (would need an admin to change it, another manual step), and an *unmerged* pull request is exactly as forgettable as the status quo — it would still sit there until the next release fails. A direct push has no human step to forget.
*Consequence:* this needs `main` to stay without branch protection or required reviews, same as today; the README now says so explicitly. If that ever changes, this job would need revisiting (back to a PR, or a required-check exemption for `github-actions[bot]`).

### D2 — New `scripts/changelog.sh backport` subcommand, not inline workflow `awk`/`sed`
Mirrors `release`'s existing split (logic in the tested script, the workflow only calls it): `backport RELEASED_FILE` takes the *other* file (e.g. `rel-<version>`'s `CHANGELOG.md`) and merges it into the current `-f` file:
1. Find `## [Unreleased]` and the next `## [` heading in `RELEASED_FILE` — that heading's bracketed text is the version just released.
2. If the current file already has a `## [<version>]` section, it's already synced: print a message to stderr and exit 0 without writing (idempotent).
3. Otherwise, take the current file's own `[Unreleased]` body (same "drop empty categories" rule as `release`), and output `RELEASED_FILE` with that body spliced in where its (empty) `[Unreleased]` body was.
Testable without GitHub, like `release` and `section`; reuses `AWK_LIB`'s `fail`/`is_entry`/`is_section_end` helpers.
*Alternative:* `git diff`/`git checkout` sequence from the README, scripted in the workflow. Rejected: duplicates the "which entries are new" logic that `release` already has in awk, in bash/git instead, for one specific call site.

### D3 — Idempotent and retried, not "exactly once"
`sync-changelog` loops up to 5 times: re-fetch `main`, re-run `backport`, commit (if `backport` changed the file — checked with `git diff --quiet`), push; sleeps `5 * attempt` seconds between tries. A push racing with another commit to `main` (e.g. an unrelated pull request merging while this release builds) fails the push with a non-fast-forward error; the next loop iteration re-fetches `main` (now including that other commit) and recomputes the backport against it, so the retry is correct, not a blind retry of a stale commit.
*Consequence:* if a maintainer somehow re-runs this job after it already succeeded, `backport`'s idempotency check (D2.2) makes the loop a no-op on the first iteration.

### D4 — Not in `cleanup`'s dependency graph
`sync-changelog`'s `needs: [prepare, build, publish]` matches `cleanup`'s, but `cleanup`'s own `needs` list is left as `[prepare, build, publish]` (unchanged) — `sync-changelog` is not added to it. GitHub Actions' bare `failure()` in a job's `if:` only considers the jobs listed in that job's `needs`, so `sync-changelog` failing cannot trigger `cleanup` and delete the just-published release, tag or branch.
*Alternative:* add `sync-changelog` to `cleanup`'s `needs` for "completeness". Rejected: that is exactly the bug this change must not introduce — the firmware release already shipped; a changelog-sync hiccup must not roll it back.

### D5 — Failure path: visible, not silent, with a named fallback
If all 5 attempts fail, the step prints `::error::` (so the run shows red in the Actions UI) naming the exact manual recovery command (`git checkout rel-<version> -- CHANGELOG.md`, same as the pre-existing fallback) and points at the README section. The workflow run's own failure is the "reminder" that was missing before — but now it is immediate (same run, visible in the Actions UI) instead of delayed to the next release attempt.

## Risks / Trade-offs

- [`main` gets branch protection later] → D1's direct push breaks; needs revisiting then (not needed today — see Non-goals).
- [A very long-running, very contested `main` (many concurrent merges) exhausts 5 retries] → falls back to the pre-existing manual pull-request recipe, now with a clear error pointing at it, rather than a silent gap.
- [Two releases started in quick succession] → already excluded by the existing `concurrency: group: release` on the whole workflow; `sync-changelog` of release N finishes (or fails loudly) before release N+1's `prepare` can run.

## Migration Plan

1. Merge this change. The next release's `sync-changelog` job runs for the first time; no change to `prepare`/`build`/`publish` or to already-published releases.
2. If the backlog from the two missed copy-backs has not already been fixed (it was, separately, in `fix-changelog-sync`), the first run after this change still benefits: it is idempotent and simply does nothing extra if `main` is already current.
3. Rollback: revert the workflow job and the `backport` subcommand; the manual copy-back recipe still works unchanged.
