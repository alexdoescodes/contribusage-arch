---
type: log
updated: 2026-10-29
---
# Log

Append-only record of what changed in the project, newest last. Entry format: `## [YYYY-MM-DD] <type> | <title>` with one to three bullets. `grep '^## \[' log.md | tail -5` shows the latest entries.

## [2026-09-28] spec | Specification 0.1
- SPEC.md drafted: provider framework, Claude Code limits and activity, GitHub contributions, tasks in six phases.

## [2026-09-28] wiki | Project memory and agent protocol
- Set up the `llm-wiki/` vault: overview, index, repo map, one page per build target, `wiki.sh` tooling with Claude Code hooks ([[decisions/0012-llm-wiki-as-project-memory]]).
- `AGENTS.md` defines the per-turn protocol and what may be published; `CLAUDE.md` imports it.

## [2026-09-28] decision | Spec and vault overlaps resolved
- ADR-001 to ADR-011 moved from SPEC §18 into `decisions/`; Appendix B replaced by `AGENTS.md` ([[decisions/0013-spec-standalone-contract]]).
- Appendix A's sample output scrubbed of skill and plugin names (SPEC §16.2 rule).

## [2026-09-28] chore | Codebase foundation and CI
- Xcode project (arm64, macOS 14, agent app) and `ContribusageKit` with the SPEC §15.4 targets, one seed type and test each (T-1.2, T-1.3; T-1.1 awaits a visual check).
- `ArchitectureTests` enforces NFR-17 on every `swift test`; CI runs tests, format, the arm64 Release build and the wiki lint ([[decisions/0014-build-and-ci-foundation]]).
- Open design points in SPEC §9/§10 listed on [[modules/core]] and [[modules/app]].

## [2026-09-28] fix | NFR-17 import scan covers attributed imports
- `ArchitectureTests` missed `@_spi(X) import` and access-level imports (`public import`); an `@_spi` import of `ContribusageGitHub` in the core compiled and passed. The pattern now matches both.
- Seed file in `ContribusageTestSupport` renamed to `ProviderID+Fake.swift` after what it holds; README gains build steps.

## [2026-09-28] fix | App build on Xcode 26: zero warnings via the test command
- CI's Release build failed: Xcode 26.6 passes `-suppress-warnings` to package targets, which conflicts with `treatAllWarnings(as: .error)` from `Package.swift`.
- The package now enforces zero warnings through `swift test ... -Xswiftc -warnings-as-errors` (AGENTS.md, SPEC §15.6, CI); tools version back to 6.1 ([[decisions/0014-build-and-ci-foundation]]).

## [2026-09-28] spec | T-1.1 and T-6.8 done
- T-1.1: the app launches as a menu bar agent (icon, no Dock icon, Quit) and the Release build is arm64 only.
- T-6.8: the CI workflow's first fully green run on a pull request ([[decisions/0014-build-and-ci-foundation]]).

## [2026-09-28] feat | T-1.4 support seams and fakes
- `ContribusageCore/Support`: the SPEC §10.6 protocols, live `SystemTimeSource` and `URLSessionTransport`; `AppPaths` is a struct with a root ([[decisions/0015-app-paths-struct]]).
- `ContribusageTestSupport/Fakes.swift`: a fake per protocol, each used in `SupportTests`; the live process runner, Keychain store and FSEvents watcher stay with T-2.4, T-3.1 and T-4.5 ([[modules/core]], [[modules/test-support]]).

## [2026-09-28] feat | T-1.5 persistence
- `JSONStore` (core, `Persistence/`): versioned atomic JSON files, folders created `0700`, version mismatch throws and never deletes; `AppPaths.deleteProviderData` for US-12.
- Files are the envelope `{"schemaVersion", "value"}`; SPEC §10.7 says so and exempts the bridge's `statusline-limits.json`.
- SPEC Appendix D: the bridge creates its folders under `umask 077`, so the root stays `0700` whoever creates it first.

## [2026-09-28] feat | T-1.6 provider framework
- Core `Providers/`: descriptor, capabilities, availability, `UsageProvider`, `LimitsSource`, `ActivitySource`, the neutral reports and `SourceError`; `ProviderRegistry` with order, first-run enablement and availability caching ([[modules/core]]).
- Test support: `FakeProvider` and `ProviderConformance`; the fake passes the suite, and a broken provider is shown to fail it ([[modules/test-support]]).
- [[decisions/0016-activity-source-stream-lifetime]]: `ActivitySource` drops `start`/`stop`; SPEC §10.2 and §16.4 follow.

## [2026-09-28] feat | T-1.7 popover shell
- Core gains `Snapshot`, `Origin`, `SourceState`, `NotConfiguredReason` (SPEC §10.1); GitHub gains the §10.5 report types.
- App: `AppState`, popover views drawing every SPEC §11.3 state, mock data and one light/dark preview per state, one and two providers.

## [2026-09-28] feat | T-2.1 usage parser
- `UsageParser` in `ContribusageClaudeCode/Limits/` implements SPEC §8.1.3 P-1 to P-9 and the §8.1.4 classification, returning core `UsageWindow`s; written from the rules because no earlier parser existed in the repository.
- Swift Testing tests with the Appendix A fixture via `Bundle.module`, covering every P-7 format, year rollover, the Berlin DST change, New Year and the unknown zone fallback.
- Review pass: the parser returns core `UsageWindow` instead of its own window type, P-7 formats carry an explicit `dated` flag, and an unused test constant is gone.

## [2026-09-29] research | R-2 usage output variants
- Logged out and API key billing both exit 0 with the same cost summary and no windows; captured as fixtures ([[research/r-2-usage-output-variants]]).
- SPEC P-10 and §13 name the exact text; logged out lands in `unsupportedPlan`.

## [2026-09-29] feat | T-2.2 limits report
- `UsageParser.report` returns the `LimitsReport` with `billingNote`, `insights` and `rawOutput` (SPEC §8.1.3 P-10, P-11); it is the parser's only public entry point; `windows(in:)` is gone, a private `window(in:)` reads one line.
- SPEC §16.2 synthetic fixtures stay inline strings in the tests.

## [2026-09-29] feat | T-2.3 Claude locator
- `ClaudeLocator` resolves `claude` in FR-6 order over `ProcessRunning` and returns the login shell's PATH with it, so npm installs validate from a Finder-started app
- Stateless by design: caching moved to the provider task T-2.5, which owns re-resolution
- `executableKind(at:)` tells arm64, x86_64 and script apart from the file header (FR-36)

## [2026-09-29] refactor | T-2.3 review pass
- `ClaudeLocator.ExecutableKind(at:)` reads header words with `UInt32(bigEndian:)`/`UInt32(littleEndian:)` instead of reversing bytes by hand
- Dropped `Equatable` from `Found` (never compared) and the explicit one on `ExecutableKind` (enums without payloads get it)

## [2026-09-29] feat | T-2.4 live process runner
- `LiveProcessRunner` in `ContribusageCore/Support/Live`: concurrent pipe reads, SIGTERM then SIGKILL on timeout or cancellation, stdin `/dev/null`
- One process at a time across all runner instances (NFR-18) through a shared FIFO gate; nine process tests against system tools
- Review pass before commit: stopping asks `Process` itself (`isRunning`, `terminate()`) instead of a lock-guarded mirror of its state; the timeout verdict is the deadline task's result

## [2026-09-29] feat | T-2.5 Claude Code provider
- `ClaudeCodeProvider` with descriptor, detection and the `UsageProbe` limits source: cached locate with re-resolution, single flight join, SPEC §13 error mapping; passes the conformance suite
- `SourceError.unsupportedPlan(note:)` added to the core for P-10 ([[decisions/0017-unsupported-plan-source-error]])

## [2026-09-29] refactor | T-2.5 review pass
- `UsageProbe` folded into `ClaudeCodeProvider`, now one actor that is its own `LimitsSource`; the launch retry is a two-pass loop
- Detection no longer reports `unsupportedPlan` from a cached probe note, which the registry could never read; P-10 and ADR-017 updated
- Second pass: the login check also matches "logged in" (whole words only); tests for an override change and each probe result mapping

## [2026-09-29] feat | T-2.6 refresh coordinator
- `Schedule.nextRun` (pure: bounds, backoff, Low Power Mode, offline, sleep, wake delay, manual floor) and `RefreshCoordinator` (serialized passes in registry order, SPEC §13 mapping, `state.json` persistence and restore)
- `SchedulePolicy.needsNetwork` and `Origin.poll` added; ADR-018 settles the coordinator shape and the GitHub seam
- App wiring and the popover/reset triggers moved into T-2.7

## [2026-09-29] refactor | T-2.6 review pass
- `RefreshCoordinator.refresh` separates the SPEC §13 mapping (a pure function) from bookkeeping; failures derive from the state
- `ProviderID` is `CodingKeyRepresentable`, so `state.json` keys need no conversion; one `Duration.timeInterval` replaces three conversions
- Third pass: `restore()` no longer relabels an in-memory snapshot as `cache` when `start()` runs again; the backoff drops a redundant branch

## [2026-09-29] feat | T-2.7 limits UI and app wiring
- The app runs live: `AppState.live()` registers `ClaudeCodeProvider`, starts `RefreshCoordinator` and feeds it sleep, wake, network and Low Power Mode through `SystemConditions`; Refresh, Retry and popover open reach the coordinator.
- Extra triggers as `Schedule.nextRun(trigger:)`: popover open and a window's reset, floored at the minimum interval (SPEC §12 rule 8, [[decisions/0019-extra-refresh-triggers]]).
- Limits section: §11.5 reset wording with hover date, "reset, refreshing…" after a reset (FR-11), §11.7 VoiceOver labels, §13 error wording.
- Fix: provider groups were invisible in the live popover (`ViewThatFits` fell back to a zero-height `ScrollView`); the scroll view now takes the groups' measured height.

## [2026-09-29] refactor | T-2.7 review pass
- `Schedule.nextRun(triggers:)` filters triggers by the last run itself; `popoverOpened()` hands `now` to its one pass instead of storing and clearing it per record ([[decisions/0019-extra-refresh-triggers]]).
- The §11.5 and FR-11 window texts moved from `LimitsSection` into the core (`UsageWindow+Display`) with tests, ready for the menu bar label (T-2.8).
- Retry is an optional closure per section (limits only for now); condition changes reach the coordinator in order through one stream.

## [2026-09-29] refactor | SystemConditions keeps no observer tokens
- `NotificationCenter` holds each block until it is removed, and `SystemConditions` lives as long as the app, so the stored token array is gone.

## [2026-09-29] spec | Menu bar label moves to M4
- T-2.8 becomes T-5.10: the menu bar label (FR-12, SPEC §11.1) follows the GitHub wiring and the display mode setting; M1 is now "limits live in the popover".
- FR-12 drops to P2; US-1 notes that its menu bar criteria arrive with T-5.10, and the M1 check covers only its popover criteria.

## [2026-09-29] fix | Probe folder created owner-only
- The first probe created `~/Library/Application Support/contribusage/` with the default `0755` before `JSONStore` could create it as `0700` (SPEC §10.7). Found during the M1 check.
- `JSONStore.createFolder` is now the one place that creates folders under the root; `JSONStore.write` and the probe both use it, and `createsTheProbeFolderOwnerOnly` covers the probe path.

## [2026-09-29] test | M1 check
- US-1's popover criteria and US-2 hold over a day of uptime without sleep; SPEC §17.2's M1 check is ticked.
- Release build sampled once a minute (`ps` CPU time, `footprint`): quiet minutes cost at most 0.01 s CPU (NFR-1), memory footprint 15 to 25 MB (NFR-2). Opening the popover adds about 8 MB and 0.5 s; a probe about 0.3 s.

## [2026-09-29] feat | T-3.1 KeychainSecretStore
- `KeychainSecretStore(service:)` in the core's `Support/Live` is the live `SecretStore`: generic passwords, key as account, after first unlock and this device only (FR-16, SPEC §14).
- `keychainSecretStoreRoundTrips` runs against the real Keychain under a throwaway service only with `CONTRIBUSAGE_LIVE_TESTS=1`; `FakeSecretStore` keeps its unit test.

## [2026-09-29] feat | T-3.2 GitHub settings tab
- `GitHubClient.viewerLogin(token:)` validates a token with `viewer { login }` (FR-17); fake-transport tests cover the request, 401, HTTP errors and GraphQL errors.
- `GitHubAccount` owns the token: only a validated token is saved, tested with fakes. New Settings scene with the GitHub tab (validate, "Connected as @login", remove); Connect GitHub in the popover opens it.
- `SourceError.message(displayName:)` is shared by popover and Settings; `unauthorized` now reads "GitHub token is invalid or expired" (SPEC §13). SPEC §10.7 gains the `githubLogin` default; T-3.2 ticked.

## [2026-09-29] feat | T-3.3 GitHubClient contributions
- `GitHubClient.contributions(token:from:to:)` returns a `ContributionCalendar`, which `GitHubReport` now carries with the stats (SPEC §10.5); one generic `send` serves it and `viewerLogin`. The query drops the unused `weekday` (Appendix C).
- Rate limits follow GitHub's GraphQL docs: remaining 0 on a failed response pauses until the reset; SPEC §8.4.3 and §13 corrected ([[decisions/0020-github-rate-limit-detection]]).
- Fixtures `github/calendar.json` and `github/graphql-errors.json`, both synthetic; SPEC §16.2 no longer asks for a real calendar, whose counts are personal data.

## [2026-09-30] feat | T-3.4 contribution statistics
- `ContributionStats(days:now:calendar:)` computes today, this week and both streaks per SPEC §8.4.4, ignoring entries after today.
- ADR-021: the caller's `Calendar` defines today and the week start, so R-4 now blocks T-3.6 instead of T-3.4; SPEC §8.4.4, §17, §19.1 updated.

## [2026-09-30] feat | T-3.6 GitHub in the refresh coordinator
- `RefreshCoordinator<GitHubValue>` runs an app-supplied `GitHubJob` in its passes, persists it under `github` in `state.json`; no token and 401 wait for the user, a rate limit runs at its reset, a token change resets inside a pass (ADR-022).
- `GitHubAccount.report(now:calendar:)` fetches the year and its statistics; the app wires it with `Calendar.current` until R-4, Settings reports token changes, GitHub errors show Retry.

## [2026-09-30] fix | Heatmap fits the popover
- The live 365-day calendar drew 53 columns and widened the popover; the heatmap now draws `ContributionCalendar.weeks(last: 26)` (FR-20 default), Sunday-aligned from the last day, with top-aligned columns.
- Mock and placeholder use real dates, so previews show the same layout as live data.
- Cells are squares sharing the content width, placed by the `WeekColumns` layout, which derives its height from the width: `aspectRatio` cells shrank to dots under the menu bar window's small height proposal.

## [2026-09-30] refactor | One job type in the refresh coordinator
- Providers' limits and GitHub are both a `Job<Value>` run by one generic `run`; no token is `SourceError.tokenMissing`; a token change replaces GitHub's record and persists (ADR-022).
- `ContributionCalendar` keeps GitHub's `weeks` (SPEC §10.5), the heatmap takes `weeks.suffix(26)`; `AppState` owns the GitHub account and tells the coordinator about token changes itself.

## [2026-09-30] feat | Change token… after a 401, fixed heatmap range, M2 done
- FR-20, US-4, §10.7, §11.6: the heatmap shows 26 weeks with no range setting; `heatmapWeeks` is gone (the 52-week option had no use). Hover tooltips appear after 0.2 s (`NSInitialToolTipDelay`); a heatmap cell's reads "2026-09-27: 5 contributions" (§11.2).
- A 401's error line offers "Change token…" instead of Retry, which opens Settings (US-4, §13, §16.5); the GitHub tab is one Account row (Disconnect) and one token field (Connect, or Replace, which keeps the saved token until the new one validates; §11.6).
- T-3.5 done with hover; its palette, keyboard and VoiceOver work moves to T-5.11 (M4). M2 checked by hand, ticked.

## [2026-10-01] feat | Structured insights area (T-6.1)
- `UsageParser` turns the "What's contributing" block into `Insights` (periods, shares, rankings; unknown lines kept); `state.json` schema 2.
- The popover shows one period at a time with a segmented picker, the top three per ranking and the note as a tooltip.
- SPEC FR-8, FR-38, US-8, P-11, §7.2, §10.3 updated; [[decisions/0023-structured-insights]].

## [2026-10-02] feat | Transcript line decoder (T-4.1)
- `TranscriptLine.decode` reads usage-bearing lines leniently; non-usage lines give nil, malformed ones throw (FR-23, SPEC §8.3.2).
- One `JSONDecoder` and one date format style are built once, since decode runs per line (NFR-7).
- Field paths confirmed against real transcripts; R-3 stays open for the ccusage comparison (T-4.7).

## [2026-10-02] feat | Transcript aggregator (T-4.2)
- `TranscriptAggregator` de-duplicates by `message.id:requestId` and buckets per local day and model (FR-24, FR-25); `TokenCounts` gained `+=`.
- Per-file removal for FR-26 is left to T-4.3.
- Review pass: `DayKey(_:calendar:)` in the core replaces two copies of the day formatting (aggregator, `ContributionStats`); the aggregator's key and token-count code shortened.

## [2026-10-02] feat | Incremental JSONL reader and transcript files (T-4.3)
- Core `IncrementalJSONLReader` returns appended complete lines per file and flags a rescan on identity change or shrink; a deleted file reads as `nil` (FR-26, SPEC §8.3.5).
- Claude Code `TranscriptFiles` lists FR-22 roots and `**/*.jsonl`, skipping the probe folder's project directory (FR-28).
- SPEC §8.3.1: project directories encode every non-alphanumeric character as `-`, not only `/`.

## [2026-10-02] feat | History store (T-4.4)
- Core `HistoryStore` keeps a provider's frozen days in `providers/<id>/history.json`: a day freezes once it ended more than 48 h ago, is never recomputed, and is kept 365 days (FR-29, SPEC §8.3.4).
- `merge(_:now:calendar:)` freezes, prunes, writes only on change and returns frozen days plus unfrozen live days younger than 48 h, each day once; an unreadable file throws and stays on disk.
- Test support: `AppPaths.temporary()` replaces three private copies of the temporary root helper.

## [2026-10-02] feat | Live file events with FSEvents (T-4.5)
- Core `FSEventsFileEvents`: one FSEvents stream per consumer, file level events, the debounce as latency, utility queue (FR-27).
- Batches report files only, by real path; a root created later is picked up; dropped events are left to the activity source's rescan.; a stream that cannot start finishes.

## [2026-10-02] feat | Claude Code activity source and Activity section (T-4.6)
- `TranscriptActivity`: roots from FR-22 plus the login shell's `CLAUDE_CONFIG_DIR`, a full incremental pass per file event or rescan, de-duplicated across files, merged with the history store; `ClaudeCodeProvider` gains the `activity` capability and passes the conformance suite with it.
- App: one `reports()` consumer per enabled provider, rescans on popover open and wake, `noLocalData` when no days exist; `ActivitySection` shows today by date, the reported token categories only and a zero-filled 7 day chart with a VoiceOver summary.
- ADR-024: the live index stays in memory; T-4.7 decides on persisting it. ADR-025: the SPEC §16.4 declared-roots read check and its file system seam move to T-4.7. `TranscriptFiles.roots` now returns missing roots too, so they are watched.

## [2026-10-02] spec | ccusage validation recorded (T-4.7)
- SPEC §8.3.6 and R-3: daily token totals match `ccusage daily` within 1 %, checked by hand against the built activity source.

## [2026-10-02] fix | Live index within NFR-2, NFR-7 measured (T-4.7)
- Opt-in performance test over a generated 520 MB tree: cold scan 1.5–1.7 s, incremental 42 ms in release on an M3; NFR-7 judged on the M3 against half its budget (ADR-026). The live index stays in memory (ADR-024), `live-index.json` left SPEC §10.7.
- Memory went from about 400 MB growth to 27 MB: the JSONL reader streams through a fixed buffer instead of autoreleased whole-file data, and the live index keeps the first line per key (SPEC §8.3.4). Findings: [[research/nfr-7-transcript-scan]].

## [2026-10-02] test | Declared-roots read check (T-4.7)
- New seam `FileReading` (SPEC §10.6): the transcript listing, the JSONL reader and the history store read through it; `LiveFileReader` in the app.
- `ProviderConformance` fails a provider that lists or reads outside its declared roots (SPEC §16.4, ADR-025), proven by `conformanceSuiteRejectsAReadOutsideTheRoots` and a mutation run.

## [2026-10-02] fix | T-4.7 review fixes
- Interning survives incremental appends; the reader returns a named `Outcome`; `JSONStore.read` takes its `FileReading` without a default.
- The read check fails when nothing went through the seam and resolves symlinks; the perf test asserts ADR-026's half budget and fails if it cannot measure memory. T-4.7 ticked.

## [2026-10-02] test | M3 check
- US-5 holds in the running app, checked by hand; SPEC §17.4's M3 check is ticked. Phase 4 is complete.

## [2026-10-02] feat | Threshold notifications (T-5.1)
- `NotificationPlanner` in the core; the coordinator plans after each polled fetch and persists the keys in `state.json` ([[decisions/0027-notification-planning-in-the-coordinator]]).
- App delivery through `UNUserNotificationCenter`; SPEC §9.2 wording and T-5.1 ticked.

## [2026-10-02] refactor | T-5.1 review fixes
- Planner stores one cycle per window (highest threshold sent) instead of a key per threshold; `state.json` stays readable; FR-15 and [[decisions/0027-notification-planning-in-the-coordinator]] updated.
- The limits job's `fetch` runs the planner (no state filter); default thresholds in one place; delegate set once; coordinator test uses the shared helpers.

## [2026-10-02] feat | Settings window (T-5.2)
- General, Providers, GitHub and Advanced tabs: enable toggles that start and stop a provider at runtime (FR-2, US-12), Claude Code's located `claude`, override with Test, probe and GitHub intervals, data deletion while off, thresholds, insights toggle, reset caches.
- Core: `Job.interval`, `limitsInterval`, `intervalsChanged()`, `resetCaches()`; `ClaudeCodeProvider.located()`. [[decisions/0028-settings-scope-and-wiring]]; SPEC FR-2, §10.7, §11.6, T-5.2, T-5.10 updated.

## [2026-10-02] fix | T-5.2 review fixes
- `restore()` publishes only for sources without a state, so enabling a provider no longer hides a rejected token or a failure (test added).
- App: a group per registered provider plus `enabledIDs` (no parking or reinsertion), one `IntervalPicker`, `Lookup` for the located `claude`, `NotificationPlanner.Settings.stored`; `ClaudeLocator.Found.kind`, `ClaudeCodeProvider.located()` replaces the private `locate()`.

## [2026-10-02] test | Notifications and Settings checked by hand
- US-3 notifications (threshold crossing on a fetch, permission prompt, banner) and the T-5.2 Settings window verified in the built `.app` from `/Applications`.
- How to repeat the check: [[modules/app#Things that bite]].

## [2026-10-03] feat | Launch at login (T-5.3)
- General tab: launch at login toggle through `SMAppService.mainApp`, status read back after each call and on appear, Open System Settings while approval is pending.
- SPEC T-5.3 ticked; [[modules/app]] updated.

## [2026-10-03] fix | T-5.3 review fixes
- Launch at login status re-read on app activation instead of on appear, since approval happens in System Settings.
- A failed register or unregister shows its error instead of being swallowed; the approval row uses the title + subtitle `LabeledContent`.

## [2026-10-03] decision | No first run onboarding (ADR-029)
- FR-37 and T-5.4 struck: an onboarding card was built and dropped in review as unintuitive for developers, who configure in Settings.
- US-6 no longer offers launch at login on first run; the §16.5 fresh user row relies on the popover's not configured states.

## [2026-10-03] feat | Copy diagnostics and copy statistics (T-5.5)
- Settings' Advanced tab copies diagnostics: app, macOS, chip, per source state, skipped lines, Claude Code's located `claude` and last probe, GitHub's state; never a token.
- The popover footer's Copy button copies what the popover shows, insights of every period included (new FR-42); its lines share the views' helpers.
- `UsageProvider.diagnostics()` with an empty default carries provider lines ([[decisions/0030-provider-diagnostics-hook]]); SPEC §10.2, §11.2, §13 and FR-36 updated.

## [2026-10-03] spec | diagnostics() may detect
- SPEC §10.2: `UsageProvider.diagnostics()` may run detection (Claude Code runs `claude --version`), never a fetch; the earlier "no process" contradicted [[decisions/0030-provider-diagnostics-hook]].

## [2026-10-03] feat | Wake, offline and Low Power Mode end to end (T-5.6)
- Wake plus 10 s is an extra trigger in `Schedule.nextRun`, as SPEC §12's table lists it; rule 8 names it.
- A source marked offline runs on reconnect within its `manualFloor`; offline no longer replaces a rate limit, so its reset still holds (rule 2).
- The coordinator logs each next pass with Low Power Mode for the §16.5 check.

## [2026-10-03] feat | Accessibility and localization pass (T-5.7)
- String Catalogs in the app and the core, English and German (an English-only app formats as `en_<region>`), checked by a CI sync step (ADR-031); percents follow the locale.
- VoiceOver: §11.7 chart summary, header traits; provider groups scroll only when they overflow, since VoiceOver stopped at the scroll area.
- Popover keeps the system's Liquid Glass; heatmap in system green at 40 to 100 %; footer shows icons with tooltips (SPEC §11.2).

## [2026-10-03] research | Performance and energy verification (T-5.8)
- Release build on an M3: 0.07 % CPU idle with the popover closed (NFR-1), 43 to 47 MB footprint (NFR-2), 3 idle wakeups per minute; NFR-3, NFR-4 and NFR-14 checked in code ([[research/t-5-8-performance-energy]]).
- A Claude Code session writing transcripts raises CPU to about 0.2 %, because each incremental pass opens every transcript.

## [2026-10-03] spec | T-5.9 moved to T-5.12
- The check of the fake provider debug build (US-11) waits for a planned popover restructure, which would replace the layout it checks.

## [2026-10-03] spec | Popover layout and shared heatmap (T-5.13 to T-5.18)
- New in SPEC: Source, Section, Block and Heatmap layer (§3); US-13, US-14; FR-43 to FR-49 (GitHub switch, blocks the user orders and hides, work follows use, shared heatmap with Combined and Stacked styles); layout types in §10.8.
- Heatmap design, levels per source and hues recorded as [[decisions/0032-shared-heatmap]]; `stats-cache.json` left open as Q-8.
- T-6.9 and the name check moved into Phase 5 (T-5.17, T-5.18); T-5.10 to T-5.12 now follow the layout tasks.

## [2026-10-03] spec | Plan display after v1 (FR-50, R-6, T-6.10)
- Providers may report their plan as the tool names it (US-15, FR-50); a provider whose data differs by plan lists what each plan delivers (SPEC §7.3).
- Claude Code's `/usage` doesn't name the plan; R-6 checks whether `claude auth status` does. Credential files stay off limits.

## [2026-10-03] feat | Popover layout and demand in the core (T-5.13)
- `PopoverLayout`, `BlockID`, `SectionKind`, `HeatmapStyle` and `Demand` (SPEC §10.8): block order with appended new providers, hidden blocks and sections, the heatmap's "on" rule, and FR-46's demand.
- Activity watching and GitHub follow the demand; the layout persists as JSON under the `popoverLayout` default (SPEC §10.7).

## [2026-10-03] feat | GitHub on/off switch (T-5.14)
- `githubEnabled` setting (SPEC §10.7) feeds the sources that are on; off stops the GitHub job and hides its section, the token stays.

## [2026-10-03] feat | Claude Code tool texts in German (ADR-033)
- `ProviderDescriptor.toolText` translates window labels, insights periods, counts and note at display time; the printed text stays the key ([[decisions/0033-provider-tool-texts]]).

## [2026-10-03] feat | Popover tab and blocks in order (T-5.15)
- The popover and Copy statistics follow `PopoverLayout`: block order, hidden blocks and sections, "Nothing to show"; Settings gets the Popover tab ([[modules/app]]).
- Advanced's `showInsights` toggle dropped: the Popover tab's Insights toggle replaces it (SPEC §10.7, §11.6, [[decisions/0028-settings-scope-and-wiring]]).

## [2026-10-03] fix | Popover tab blocks can be dragged
- `onMove` does nothing in a grouped `Form`; rows are now draggable and drop targets, and `PopoverLayout.move(_:to:registered:)` puts the dragged block in the target's place.

## [2026-10-03] decision | Sections reorder within their group (ADR-034)
- `PopoverLayout.sectionOrder` and drag within a provider row; the popover and Copy follow it. The Popover tab's drag gets a handle, a card preview, a lit drop target and sliding rows (none under Reduce Motion).

## [2026-10-03] feat | Shared heatmap block (T-5.16)
- Core: `HeatmapLayer` (quartile levels, 26 week range, FR-49 line), `PopoverLayout.heatmapLayers`, `ProviderDescriptor.heatmapHue`.
- App: `HeatmapBlock` in Combined and Stacked styles with legend and tooltips; the grid left GitHub's block; Copy lists each layer's total; the Popover tab previews the style.

## [2026-10-03] spec | Combined cells stripe only active layers; no month labels
- FR-49: a Combined cell splits only among the layers active that day, so one active source fills the cell; month labels dropped.

## [2026-10-03] refactor | One switch builds each heatmap layer
- `HeatmapLayer` carries its `name`; `AppState.heatmap(at:)` returns `ShownLayer`s with hue and dimming, so the per-layer `name`, `color` and `isDimmed` lookups are gone.

## [2026-10-03] fix | Heatmap test no longer reads a catalog plural
- CI's SwiftPM leaves the core's String Catalog uncompiled, so "1 contribution" read "1 contributions"; the assertion is dropped, the catalog step covers the key.

## [2026-10-03] feat | Menu bar label and its settings (T-5.10)
- Core `MenuBarMode` and `MenuBarLabel`: modes, fallbacks, stale `~`, gauge variants, stable width ([[modules/core]]).
- App `MenuBarItem`, `AppState.menuBarLabel(at:)`, Settings' Menu bar section; a GitHub mode puts GitHub in the demand ([[modules/app]]).
- SPEC: US-1 stale wording, FR-12 not-offered mode, §11.1 GitHub mode text.

## [2026-10-03] refactor | Menu bar label review
- `MenuBarMode.usesProviders`/`usesGitHub` replace the mode lists in `offered` and the app's demand; one `limits(_:)` path for every limits mode, no recursive init.
- The menu bar provider fallback lives once, in `AppState.shownMenuBarProvider` ([[modules/app]], [[modules/core]]).

## [2026-10-03] fix | Menu bar label no longer loops on launch
- A `TimelineView` in the `MenuBarExtra` label made SwiftUI re-request label updates endlessly: 100 % CPU, memory growing without bound, no menu bar item. A `.task` loop now ticks a `@State` date each minute ([[modules/app]]).

## [2026-10-04] feat | Menu bar styles and colors (T-5.19)
- The label is one drawn, colored image: the `prompt` mark, rings, ring, line, heatmap and text; provider, usage, accent, monochrome or custom color (FR-51, [[decisions/0035-drawn-menu-bar-label]]).
- `MenuBarLabel` carries meter, companion window, stale flag, title and source instead of a gauge symbol; `iconOnly` keeps the meter.
- Settings shows each style as a live preview tile; the heatmap style watches the provider's activity (FR-46).

## [2026-10-04] feat | Shared heatmap in the menu bar
- New `sharedHeatmap` style: the shared heatmap's layers (for example Claude Code and GitHub) over the last three weeks, a stripe per active layer as in FR-49 Combined (FR-51, [[decisions/0035-drawn-menu-bar-label]]).
- The heatmap styles keep their hues from 90 %; the value text turns red instead, since red beside GitHub's green fails ADR-032's rule.

## [2026-10-04] refactor | Menu bar label review
- One rule for the layers a heatmap style draws, `menuBarHeatmapSources(_:style:)`, read by the image and the demand ([[decisions/0035-drawn-menu-bar-label]]).
- `MenuBarArt`: one value-based meter color, straight bars instead of path trimming, layers captured by the glyph; every style renders byte-identical in light and dark.

## [2026-10-04] feat | T-5.11 heatmap keyboard and VoiceOver
- The heatmap grids are one focusable control: arrow keys move the reached day through `HeatmapLayer.step`, which is outlined and read as its FR-49 line under the grids.
- Every day is a VoiceOver element read as its line; the container reads SPEC §11.7's summary with `HeatmapLayer.total`, shared with Copy; Stacked exposes its first grid only.
- Review: the reached day is kept as a `DayKey`, so it stays on its date when the range moves on.

## [2026-10-04] feat | T-5.12 pushed limits and the debug fake provider
- `RefreshCoordinator` consumes every enabled provider's `pushedUpdates()`: a push is a success with origin `push`, persisted, planned and shown (SPEC §12 rule 9).
- `App/DebugFakeProvider.swift` under `CONTRIBUSAGE_FAKE_PROVIDER` registers US-11's second provider ([[decisions/0036-pushed-limits-and-debug-fake-provider]]).
- Checked by hand in the flagged Debug build (SPEC §16.5 row); T-5.12 ticked.

## [2026-10-04] research | T-5.17 second provider evaluation
- [[research/t-5-17-second-provider]]: Codex CLI passes the gate for limits and activity through its own session files; Copilot CLI only for activity; Gemini CLI not assessed.
- [[decisions/0037-no-second-provider-for-v1]]: no second provider for v1; Codex CLI is the next candidate. SPEC T-5.17 ticked, Q-7 answered, §7.6 updated.

## [2026-10-04] research | T-5.18 name availability
- "contribusage" is free: App Stores, five domains, GitHub, package registries, web, USPTO and TMview ([[research/t-5-18-name-availability]]).
- SPEC T-5.18 ticked, Q-6 answered for the name; ADR-011 stands.

## [2026-10-04] spec | Notification fixes, one-minute probe, 7 day cost
- T-5.20 (FR-13, FR-15, new FR-52, US-3; [[decisions/0038-notification-cycles-and-delivery]]): false "has reset" and repeated threshold notifications traced to the exact reset time match; tolerance, highest threshold only, spaced and grouped delivery.
- T-5.21 ([[decisions/0039-one-minute-probe]]): probe default and minimum 1 min (NFR-5, §12 rule 10), R-1 and NFR-1 checked in the task.
- T-6.4 and FR-40 cover the last 7 days: tokens per category and money per model.

## [2026-10-04] feat | T-5.21 probe from one minute, R-1 answered
- Claude Code's `limitsPolicy`: 5 min default, 1 min minimum, 60 min maximum; Settings offers 1 to 60 min ([[decisions/0039-one-minute-probe]] revised).
- [[research/r-1-probe-cost]]: a probe costs no plan quota but about 1.7 s CPU, hence the 5 min default; the app probing every minute stays at 0.08 % CPU (NFR-1).

## [2026-10-04] refactor | Interval choices from the policy
- `IntervalPicker` derives its choices from fixed steps within the `SchedulePolicy` bounds; the Claude Code and GitHub callers no longer repeat the bounds ([[modules/app]]).
- The T-5.21 test asserts the policy's default and minimum directly; Low Power Mode doubling stays covered in `ScheduleTests`.

## [2026-10-04] fix | T-5.20 notification cycles and delivery
- `NotificationPlanner.plan`: a cycle ends only on a reset time move of more than 1 h, keeps its first known reset time (one identifier per window and cycle) and notifies only the highest threshold crossed ([[decisions/0038-notification-cycles-and-delivery]]).
- `NotificationDelivery` queues notes and posts them 3 s apart in one thread per provider; the coordinator's `deliver` closure is synchronous now, so a refresh never waits for delivery ([[modules/app]]).
- Spec: US-3 says "at most one" per threshold; FR-15 names the cycle's first reset time as its key and ends a cycle when the window reports no reset time after the cycle's has passed.

## [2026-10-04] fix | T-5.22 menu bar polish
- GitHub modes fill the glyph with today's contribution level and the inner ring with the week's active days, never red ([[decisions/0035-drawn-menu-bar-label]]).
- `iconOnly` keeps the companion ring; the value is padded in front to two digits, so no space trails the label.
- Rings and ring drawn at 15 and 16 pt; SPEC FR-12, FR-51, §11.1 and T-5.22 updated.

## [2026-10-04] wiki | Staleness by day
- `wiki.sh lint` flags a page when a tracked path changed on a later day than its `updated:` date, so a page that still holds needs only that date; ADR-030's "T-… changed X only in Y" notes are gone. Editing a page without bumping `updated:` no longer clears it (it caught ADR-029, edited in T-5.20).
- `test-wiki.sh` covers both cases and starts from the session template instead of the machine's latest handover.

## [2026-10-04] chore | MIT license
- `LICENSE` (MIT) added and listed in the repo map.

## [2026-10-04] feat | T-5.23 app icon and README
- `App/AppIcon.icon`: the prompt, a filling cursor line and the agent's spark in Liquid Glass, chosen over five render rounds (ADR-040); the target names it, `actool` adds the macOS 14 and 15 fallback; Claude Code's SF Symbol is `sparkle`, the icon's spark, in place of `terminal`.
- README rewritten: header SVGs in `docs/assets/`, badges, screenshots of the real popover and menu bar styles with mock data, acknowledgements, trademark note.
- SPEC: T-5.23 added and ticked, the icon moved out of T-6.7, Q-6 answered.

## [2026-10-04] feat | T-5.24 About tab
- Settings gains an About tab: app icon, version and build, copyright, links to source, license and acknowledgements (SPEC §11.6).
- The copyright comes from `INFOPLIST_KEY_NSHumanReadableCopyright`; [[modules/app]] updated.

## [2026-10-04] spec | M4 check passed
- SPEC §13 and §16.5 drop the x86_64-without-Rosetta row: such a `claude` fails FR-6's `--version` check and shows as not found; §19 and [[decisions/0009-apple-silicon-only]] say so.
- The remaining §16.5 rows were traced to code (Low Power Mode log line in the refresh coordinator, day buckets re-aggregated with the autoupdating calendar on each transcript change); the matrix itself runs by hand.
- Every §16.5 row passed by hand; M4 ticked in SPEC §17.5.

## [2026-10-04] chore | Version 1.0
- `MARKETING_VERSION` 0.1.0 → 1.0 in both configurations after the M4 check; build number stays 1. The About tab and diagnostics read it from the bundle.

## [2026-10-04] spec | R-3 closed
- R-3 checked off: SPEC §8.3 drops its "verify in R-3" caveats (`<synthetic>` lines carry zero usage and are skipped, `cleanupPeriodDays` defaults to 30 days).

## [2026-10-04] chore | Release on version bump
- `ci.yml` gains a `release` job: on `main`, after the other jobs pass, a new `MARKETING_VERSION` becomes the GitHub release `v<version>` with generated notes and the ad-hoc signed arm64 DMG.
- [[decisions/0014-build-and-ci-foundation]] records it; AGENTS.md's CI row and SPEC T-6.8 mention it.

## [2026-10-04] docs | Install from the DMG
- README's Install section: download the DMG, then clear the quarantine flag once (`xattr -dr com.apple.quarantine`) or use Open Anyway, since the app is not notarized.
- T-6.7 keeps Developer ID signing and notarization for a later stage; [[decisions/0006-no-app-sandbox]] amended, the README roadmap says so.
- README's license badge is a static MIT badge: the shields.io GitHub lookup had cached "not specified" from before `LICENSE` existed.

## [2026-10-04] docs | Contributor guide
- `CONTRIBUTING.md`, kept short and open: issues and pull requests without prior sign-off, build and test commands, the SPEC §2 rules in brief, what CI asks for (German translations, stale wiki pages, both of which maintainers can finish), coding agents via `AGENTS.md`, the provider gate for new tools.
- [[architecture/repo-map]] lists it.
- `docs/assets/social-preview.svg` and `.png`: the repository's 1280×640 social preview, laid out from the README header's icon, title, tagline and pill; the PNG is rendered with headless Chrome and uploaded by hand.

## [2026-10-04] docs | Community files
- `CODE_OF_CONDUCT.md`: Contributor Covenant 2.1; reports go through GitHub's "Report content → Report to repository admins", so no address is published.
- `SECURITY.md`: latest release supported, reports through private vulnerability reporting, the SPEC §2 and §14 promises as the definition of a vulnerability.
- `.github/ISSUE_TEMPLATE/` (bug, `/usage` output change per SPEC §16.6, feature with the provider-gate question, security contact link) and `.github/pull_request_template.md`; CONTRIBUTING.md links the Code of Conduct and the security policy. [[architecture/repo-map]] lists them.

## [2026-10-04] fix | Release job hardened
- The `release` job's `hdiutil` retries overwrite a partial image and fail the job after the third try; only its two `gh` steps get the token, and its checkout keeps no credentials.
- Runs on `main` queue instead of cancelling each other, so a release cannot stop half-created; [[decisions/0014-build-and-ci-foundation]] updated.
- [[modules/claude-code]] no longer cites the SPEC §13 Rosetta row the M4 check removed.

## [2026-10-04] chore | Release build provenance
- The `release` job attests each DMG with `actions/attest` before creating the release; `SECURITY.md` shows the `gh attestation verify` check and the README links it.
- [[decisions/0014-build-and-ci-foundation]] and SPEC T-6.8 say so.

## [2026-10-04] chore | Agent skills configuration
- `docs/agents/` holds the issue tracker (GitHub Issues on the fork, SPEC §17 stays the contract), the five default triage labels and the domain-docs rules; AGENTS.md § Agent skills points at them.
- Domain docs map to the vault: ADRs in `decisions/`, a glossary at `concepts/glossary.md` once one is needed.

## [2026-10-04] docs | Linux port design
- `docs/superpowers/specs/2026-10-04-linux-plasmoid-port-design.md`: this fork becomes a KDE Plasma widget; a C++/Qt logic library as a QML module inside plasmashell, chosen for the least system resources.
- Milestones M0 to M7; M0 rewrites SPEC.md and AGENTS.md for Linux and records ADR-041.

## [2026-10-04] chore | Linux foundation (L-0)
- CMake project with `contribusage_core` and the plasmoid `org.contribusage.widget`; Qt Test suites under `tests/`; CI in an Arch container.
- The macOS tree is removed (tag `macos-reference`, commit `2603a21`); AGENTS.md, SPEC.md and the README describe the Linux port; ADR-041 records the decision.

## [2026-10-04] docs | L-4a plan: base look first
- `docs/superpowers/plans/2026-10-04-l4a-base-look-mock-data.md`: the panel label and the popup in the macOS look, on mock data, ahead of the logic milestones L-1 to L-3.

## [2026-10-04] feat | Base look with mock data (L-4a)
- The `prompt` panel label and the popup's blocks in the macOS look, filled with the macOS preview data through new core display and heatmap functions.
- Built ahead of L-1 to L-3 so the look can be checked early; `Theme.qml` holds every color and size for the customization layers.
- Review fixes: a window past its reset draws an empty, non-red bar (FR-11, `barFraction`/`meterLevel` with the reset time); the Insights periods sit in one `TabBar`.

## [2026-10-04] decision | One week bar
- ADR-042: the popup shows `Current week (all models)` as "Current week" and hides per-model week windows; parsing keeps them (`shownWindowLabel` in the core).

## [2026-10-04] decision | Hidden week windows send no notifications
- ADR-042: a per-model week window that the popup hides also triggers no notification; L-5 implements it.

## [2026-10-04] docs | L-1a plan
- `docs/superpowers/plans/2026-10-04-l1a-models-persistence-process.md`: report models with JSON, snapshots, the XDG data folder with atomic JSON files, and a non-blocking process runner; L-1 split into L-1a to L-1c.

## [2026-10-04] feat | Core models, persistence and process runner (L-1a)
- `reports.h`, `snapshot.h`, `apppaths.h`, `jsonstore.h` and `process.h` in `contribusage_core`, with the Swift tests ported to Qt Test.
- L-1 is split into L-1a, L-1b and L-1c in SPEC §17.8.

## [2026-10-05] fix | L-1a review fixes
- Report JSON rejects wrongly typed fields and insights without periods; the process runner drops results for a destroyed context object.

## [2026-10-05] docs | L-2a plan: real Claude Code limits
- `docs/superpowers/plans/2026-10-05-l2a-claude-code-limits.md`: parser, locator, probe, cache and the widget on real limits; Claude Code 2.1.289 prints reset times as "Oct 5, 6am", which P-7 did not cover.

## [2026-10-05] feat | Real Claude Code limits (L-2a)
- `src/claudecode/`: parser, locator, probe, limits job and view; the widget shows the real limits and insights, cached in `state.json`.
- Claude Code 2.1.289 prints reset times as "Oct 5, 6am"; P-7 gained the comma forms. L-2c records synced transcripts from another machine as a later task.

## [2026-10-05] fix | L-2a review fix
- The Insights open state and period survive the minute refresh in live mode.

## [2026-10-05] docs | L-3 plan: GitHub contributions
- `docs/superpowers/plans/2026-10-05-l3-github.md`: the GraphQL client, statistics, the token in KWallet after validation, a 30 min job and an inline token form.

## [2026-10-05] feat | GitHub contributions (L-3)
- `src/github/` and `src/platform/`: the GraphQL client, statistics, the account, a 30 min job and the view; the token in KWallet after validation.
- The popup's GitHub block takes the token inline (ADR-043) and shows the green heatmap layer.

## [2026-10-05] fix | L-3 review fixes
- A token change drops a fetch still running and the old account in `state.json` (ADR-022); a refused disconnect shows as a failure.
- The token form returns after a 401 (ADR-043); a missing token or a refused wallet pauses polling until the token changes.


## [2026-10-05] feat | Claude Code activity (L-1b, L-2b)
- `src/core/`: incremental JSONL reader, history store, recursive debounced directory watcher, layered heatmap view.
- `src/claudecode/`: transcript decoding, discovery and live index on a worker thread (ADR-044); the widget shows today, the week and the Claude Code heatmap layer.

## [2026-10-05] fix | L-2b review fixes
- The directory watcher scans only the folder that changed and keeps its watch sets, so a change costs milliseconds on the GUI thread instead of a whole-tree walk; failed inotify watches are logged.
- A day is frozen into `history.json` only by a scan that read every transcript.


## [2026-10-05] feat | Schedule, refresh and planning (L-1c)
- `src/core/`: SPEC §12's `nextRun`, a `SourceSchedule` per polled source (ADR-045), the notification planner and the menu bar label.
- The limits and GitHub jobs follow their §12 policies with backoff, triggers and the manual floor; suspend and network come from logind and QNetworkInformation; the Refresh button works.

## [2026-10-05] fix | L-1c review fixes
- The 6 h unsupported-plan re-check is an interval override, not a hold, so Refresh still runs; a source back online runs within its manual floor, and an offline failure does not grow the backoff.
- A run that never answers ends after 2 min; a late answer for an ended run is ignored.


## [2026-10-05] feat | Widget states, label styles, heatmap keys (L-4)
- SPEC §11.3's states in the popup: skeletons, Retry, Locate…, Show raw output, no local data, dimmed stale layers.
- The panel label in all seven FR-51 styles and tints from the plasmoid configuration; the heatmap walks with the arrow keys.

## [2026-10-05] fix | L-4 review fixes
- The heatmap's focused day stays inside the grid when the cells change; its line never reads past the end.
- A failed activity scan shows its error and Retry (dimmed previous numbers after earlier data) instead of a lasting skeleton; the text label style shows the mark in a vertical panel.


## [2026-10-05] feat | Notifications and settings (L-5a)
- Limit notifications reach the desktop once per threshold and cycle, also across restarts (ADR-046).
- Settings pages for the label, notifications, Claude Code, GitHub and data, applied live from the plasmoid configuration.

## [2026-10-05] fix | L-5a review fixes
- A token change while GitHub is off keeps it off and hidden.
- "Delete data for Claude Code" drops the cached limits and notification cycles and has the activity worker delete its own history.json, so no thread races the file.


## [2026-10-05] feat | Popup layout, label GitHub data, Low Power (L-5b)
- The popup follows a saved layout (order, visibility, sections, heatmap membership and style, "Nothing to show"), and the activity watcher and GitHub run only while something shows them.
- The GitHub label modes show today's contributions; power-profiles-daemon's power saver doubles the intervals.

## [2026-10-05] fix | L-5b review fix
- Demand (FR-46) suspends GitHub's schedule instead of resetting it, so hiding and showing GitHub keeps backoff and rate-limit holds; the user's switch alone resets it.

## [2026-10-07] feat | Customization (L-6)
- Looks macOS, Plasma and Custom with validated token overrides and a compact density, from the new Appearance settings page.
- A QML file in the widget's data folder replaces a popup block; one that fails to load falls back to the built-in block with a notice.

## [2026-10-07] feat | Arch package (L-7, in progress)
- `packaging/arch/PKGBUILD` builds this checkout as `plasma6-applets-contribusage` and runs the tests in `check()`; it depends on the libraries the plugin links (libstdc++ and glibc after the gcc-libs split) and on kirigami and kcmutils for the QML; an `install_layout` test pins the installed files.
- The minute tick runs only while the popup is open; the README gains an install section.

## [2026-10-07] fix | L-6 review fixes
- `OverridableBlock` loads an override declaratively, so data that arrives late still loads it, and falls back with a notice when the override lacks the block's property; the override tests check the data and the notice's visibility.
- The Plasma look takes the theme's neutral, negative and positive colors as well; ADR-047 records what the customization layers leave out.

## [2026-10-13] feat | One malloc arena for the widget's threads
- `PopupModel` sets `mallopt(M_ARENA_MAX, 1)`; with the arena cap set through the environment the widget's private memory over the baseline dropped from 18-20 MB to 8-13 MB (two runs, 60 s each).
- The in-plugin measurement and the L-7 research page follow.

## [2026-10-13] feat | Arena cap moved to a static initializer
- `mallopt(M_ARENA_MAX, 1)` runs when the plugin loads instead of in the `PopupModel` constructor; the constructor call measured a 13-18 MB gap over the baseline, the environment variable 8-13 MB.

## [2026-10-15] research | L-7 resource budget measured
- Memory over a compiled KDE applet: 16 MB median (Private_Dirty), CPU 0.09 % idle, refresh under 1 ms; the arena cap, `malloc_trim` and hashed index keys gained nothing beyond about 4 MB of noise.
- ADR-048 sets the memory budget to 20 MB; SPEC §17.8 L-7 ticked.

## [2026-10-29] decision | No text marker for a stale label value
- ADR-049: the panel label no longer prefixes `~` to a stale value (`23
## [2026-10-29] decision | No text marker for a stale label value
- ADR-049: the panel label no longer prefixes a tilde to a stale value ("23%", "7"); staleness stays in `MenuBarLabel::stale` and the half-strength meter. SPEC US-1 and §11.1 changed; tests updated first.

## [2026-10-29] feat | Themes for the widget: System, macOS, installed Plasma styles
- The Appearance page's Theme replaces Look ([[decisions/0050-widget-themes]], SPEC L-8): macOS draws the macOS app's capsule meters and 2 px corners (stripes included), System uses Plasma's meter image, an installed Plasma style draws its background, meter and colors in this widget only.
- `StyleTint` carries a style's colors to every item, since Kirigami keeps custom colors on the setting item.

## [2026-10-29] feat | A Plasma style restyles the whole popup; macOS draws the app's popover
- `StyleTint` points every Plasma image inside the popup window (frame, meters, controls) at the chosen style's files ([[decisions/0050-widget-themes]]).
- The macOS theme draws its rounded popover card, the app's symbols and label colors, the Insights chevron and 60 % day bars.

## [2026-10-29] feat | Global themes and per-widget icon themes
- The Theme box lists installed global themes: one pick sets the widget's Plasma style, color scheme and icon theme ([[decisions/0050-widget-themes]], SPEC L-8).
- Icons resolve in the chosen icon theme and its inherited themes; macOS takes WhiteSur's icons when installed, else the drawn symbols.

## [2026-10-29] research | Installed themes cannot change the popup's layout
- Checked in the viewer: a global theme changes frame, colors and icons only; layout, fonts and shapes stay the widget's ([[decisions/0050-widget-themes#Consequences]]). `main.qml` and `main.xml` were edited for the check and restored unchanged.

## [2026-10-29] feat | The macOS theme matches the macOS app's popover
- Drawn symbols in the label color, see-through card on the desktop's blur, SF Pro's row height, legend and footer sizes, checked side by side with `docs/assets/screenshot*.png` ([[decisions/0050-widget-themes]]).
- Fixed: the macOS colors were undefined in dark mode (object literal after `?`), so the real popup never got them.
