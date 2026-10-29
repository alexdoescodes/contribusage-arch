# contribusage: Specification

> A KDE Plasma 6 widget for Arch Linux that shows the usage of your AI coding tools (starting with Claude Code) and your GitHub contributions at a glance. Ported from the macOS app (ADR-041).

| | |
|---|---|
| **Platform** | Arch Linux, KDE Plasma 6 (Wayland) |
| **Language** | C++23, Qt 6, QML |
| **Providers in v1** | Claude Code (more AI coding tools later, each starting with a section in §8) |

This file is the contract: what must be built. How it is built and why lives in [`llm-wiki/`](llm-wiki/). Requirement IDs (`FR-`, `NFR-`, `US-`, `P-`) are stable: new requirements take the next free number, removed ones are struck through, not renumbered. Where the code departs from this file, change the file in the same commit. The macOS-era sections (provider model, architecture, Swift interfaces, setup, test strategy, task plan, appendices) were removed on 2026-10-30; the full text is at the git tag `macos-reference` (`2603a21`) and in the history before that date.

---
## 1. Vision, goals and non-goals

### 1.1 Vision

One glance at the menu bar tells me how much of my AI coding plan I have left and when it resets. One click shows my coding activity on this Mac and my GitHub contribution graph. The app is quiet, fast, private, native to Apple Silicon and never puts my accounts at risk. Today it knows Claude Code; tomorrow it can learn other AI coding tools without a rewrite.

### 1.2 Goals

| ID | Goal |
|---|---|
| G-1 | Show the current Claude Code plan usage windows (session, weekly, any additional window) with percentage and reset time. |
| G-2 | Show Claude Code activity from this Mac: requests, sessions and tokens for today and the last 7 days. |
| G-3 | Show the GitHub contribution calendar, today's count and the current streak. |
| G-4 | Warn me before I hit a limit (notifications at configurable thresholds). |
| G-5 | Use only official, permitted interfaces of every integrated tool. Never touch credentials that belong to other apps. |
| G-6 | Be a good citizen on Apple Silicon Macs: native arm64 binary, negligible CPU and battery use, native look, dark mode, accessibility. |
| G-7 | Be provider ready: adding another AI coding tool means adding one provider module and one spec section, without changing other providers, the scheduler, the notification logic or existing persistence formats. |

### 1.3 Non-goals (for v1)

- **No macOS or Windows version in this fork.** The macOS app is at the git tag `macos-reference` (commit `2603a21` on `main`) ([ADR-041](llm-wiki/decisions/0041-linux-plasmoid-port.md)).
- No desktop environments other than KDE Plasma 6.
- **No AI coding tools other than Claude Code in v1.** The provider framework exists from day one, but further providers are future work (7.6).
- No multi-account support (one login per provider, one GitHub account).
- No data from claude.ai chat history, other devices, or tools that are not integrated as a provider.
- No team or organization dashboards, no Admin API usage.
- No cloud sync, no telemetry, no backend server.
- No plan display in v1: showing the plan a tool runs on comes after v1 (FR-50).
- No exact billing. Any "API-equivalent cost" is an estimate and optional ([FR-40](#58-optional-features-p3)).
- No combined or summed limits across providers. Each tool's limits are shown as that tool reports them.

---

## 2. Constitution: non-negotiable principles

These principles override any task, shortcut or "quick fix". If a task conflicts with a principle, the task is wrong.

1. **Official interfaces only.** Data about an AI coding tool comes exclusively from that tool's official, unmodified user facing interfaces (its CLI, its documented hooks or status features) and from files the tool writes on this Mac. For Claude Code this means the official `claude` CLI, its documented status line feature and the transcripts it writes. GitHub data comes exclusively from GitHub's official API.
2. **Never touch other apps' credentials.** The app never reads, copies, stores or forwards any tool's OAuth tokens, API keys, Keychain items, credential files, browser cookies or session tokens. It never calls a tool vendor's backend directly. For Claude Code: no access to its Keychain item or `~/.claude/.credentials.json`, no calls to `api.anthropic.com` or `claude.ai`.
3. **Local first and private.** All data stays on this Mac. The only network destination the app itself contacts is `api.github.com`. No analytics, no crash reporters that phone home.
4. **Honest data.** Every value shows its age, its source and its provider. Unknown is shown as unknown, never as `0 %`. Stale data is visibly marked as stale.
5. **Frugal.** Event driven wherever possible. Background work is bounded, scheduled with tolerance and paused when pointless (asleep, offline, Low Power Mode). Adding providers must not multiply idle cost.
6. **Fail soft.** One broken source or provider never breaks the others. Parsing failures degrade to "show raw text", not to crashes or blank UIs.
7. **Testable core.** All parsing and logic lives in the `contribusage_core` library (`src/core/` and its provider and GitHub directories), is covered by fixture based Qt Test suites, and is written test first.
8. **Provider isolation.** Everything specific to one tool lives in that tool's provider module. The core and the UI know only the provider protocols. The core never imports a provider module; this is enforced by the package's target graph.
9. **Small increments.** One task, one focused change, tests green before the next task.

### 2.1 Why principle 2 matters (Claude Code)

Anthropic's Claude Code legal and compliance page states that OAuth sign-in is meant for Claude Code and Anthropic's own apps, that developers may not collect, store or intermediate Claude.ai credentials or session tokens, and that Anthropic may enforce this without prior notice. Many community tools read Claude Code's token and call an undocumented usage endpoint. This app deliberately does not. Running the official `claude` binary with the user's own login is ordinary use of Claude Code.

This is the project's own reading of the terms, not legal advice. Reference: <https://code.claude.com/docs/en/legal-and-compliance>

Every future provider needs an equivalent paragraph for its own vendor's terms before any code is written ([2.4](#24-provider-gate)).

### 2.2 GitHub

The app uses a personal access token that the user creates and pastes in (or imports from the GitHub CLI with consent). It stores the token only in the macOS Keychain and requests only read access.

### 2.3 Naming and trademarks

- The product name is **contribusage**, always written in lowercase in the UI and documentation.
- The name, icon and bundle identifier must not contain "Claude", "Anthropic", "GitHub" or the name of any other integrated tool or vendor, and must not use their logos. The bundle identifier follows the pattern `dev.<yourname>.contribusage`.
- Provider and service names appear only as plain descriptive text ("Claude Code", "GitHub"). Each provider gets a neutral SF Symbol in the UI, never a vendor logo.
- Before any public release, check the availability of the name (App Store and web search, domain, trademark databases), tracked as Q-6.

### 2.4 Provider gate

Before any code for a new AI coding tool is written, all of the following must be done and committed to this spec:

1. **Terms review.** Read the vendor's terms regarding automation, third party tools and credentials. Summarize them in the provider section, with links, in the same style as 2.1.
2. **Interface inventory.** List the official interfaces that expose usage or limits (CLI commands, documented hooks, status features, local files the tool writes). Mark each as documented or undocumented.
3. **Compliance decision.** For each capability (limits, activity, insights), decide whether it can be built within principles 1 and 2. A capability that can only be built by reading credentials or calling undocumented vendor endpoints **is not built**. Record the decision as an ADR.
4. **Research items.** Add research items (output variants, file formats, cost of polling) to [section 19](#19-open-questions-and-risks).
5. **Provider section.** Add a section under [section 8](#8-data-sources) following the shape of §8.1 and §8.4, including window classification and token categories.
6. **Tasks.** Track the work as GitHub issues.

---

## 4. User stories and acceptance criteria

Priorities: **P1** = MVP (Milestones M1 and M2), **P2** = v1.0, **P3** = later / optional.

### US-1 (P1): See my Claude Code limits at a glance

*As a Claude subscriber, I want to see my current session usage in the menu bar so I know whether I can start a big task.*

- **Given** Claude Code is installed and logged in with a subscription, **when** the app has completed a probe, **then** the menu bar shows the primary window's percentage (default: the Claude Code window of kind `session`, "Current session"), for example `23%`.
- **Given** no probe has succeeded yet, **then** the menu bar shows the icon with `?` and the popover explains why.
- **Given** the last successful probe is older than the staleness threshold, **then** the menu bar marks the value stale (meter at half strength, 11.1), the popover dims it and says "updated 42 min ago".

The menu bar criteria arrive with FR-12 in M4 (T-5.10); until then the menu bar shows a static icon and the popover carries US-1.

### US-2 (P1): Understand when limits reset

*As a user close to a limit, I want to see when each window resets.*

- **Given** a usage window with a reset time, **then** the popover shows a progress bar, the percentage, a relative countdown ("resets in 2 h 10 min") and the absolute local time on hover ("Mon 28 Sep, 04:09").
- **Given** the reset time has passed, **then** the window is shown as "reset, refreshing…" and a refresh of that provider's limits is scheduled immediately (subject to the minimum interval).
- Reset times from the Claude Code output (which includes an IANA time zone) are converted to the Mac's current time zone for display.

### US-3 (P2): Get warned before hitting a limit

- **Given** notifications are enabled and a window crosses a threshold (defaults 80 % and 95 %), **then** at most one notification is sent per threshold per window per reset cycle; a refresh that crosses several sends only the highest (FR-13). The notification names the provider and the window ("Claude Code: Current session at 80 %").
- **Given** the window resets, **then** thresholds re-arm. An optional "limit has reset" notification is off by default.
- **Given** a tool prints a window's reset time slightly differently between two refreshes ("6:59pm", then "7pm"), **then** it is the same cycle: no "has reset" notification and no threshold notification a second time (FR-15).
- **Given** one refresh brings notifications for several windows, **then** they appear one after another a few seconds apart, and Notification Center groups each provider's notifications (FR-52).

### US-4 (P1): See my GitHub contributions

- **Given** a valid GitHub token is stored, **then** the popover shows GitHub's layer in the shared heatmap of the last 26 weeks (US-14), today's count, the current streak and the total for the last year.
- **Given** no token is configured, **then** the GitHub section shows a token form: field, Connect, link to create a token (ADR-043).
- **Given** the token is invalid or revoked, **then** the section says so and offers "Change token…", which opens Settings. Other sections keep working.

### US-5 (P2): See my Claude Code activity on this Mac

- **Given** transcripts exist, **then** the popover shows for today: requests, sessions, input tokens, output tokens, cache read and cache write tokens, and a 7 day bar chart of total tokens.
- The numbers match `ccusage daily` (same dates, same machine) within 1 % for token totals (validated in research item R-3).
- New activity appears within 10 seconds of Claude Code writing it, while the popover is open.

### US-6 (P2): Start automatically, run natively and stay out of the way

- The app has no Dock icon and no app switcher entry.
- "Launch at login" is a toggle in Settings (off by default; not offered on first run, ADR-029).
- The app runs natively on Apple Silicon; its executable contains only an arm64 slice.
- The app uses no measurable CPU while idle (see [NFR-1](#6-non-functional-requirements)).

### US-7 (P2): Configure everything in one place

- A Settings window lets me: enable or disable providers, paste/validate/remove the GitHub token, see and override the path to `claude`, choose what the menu bar shows, set refresh intervals within allowed bounds, set notification thresholds, toggle launch at login, and open the data folder. It also lets me arrange the popover (US-13).

### US-8 (P3): See what is driving my Claude Code usage

- **Given** the `/usage` output contains the "What's contributing to your limits usage?" section, **then** the popover can show it in a collapsible "Insights" area: one period at a time (for example last 24 h or last 7 days), its request and session summary, its shares of usage and the top three entries of each ranked list, with the CLI's own note that it is approximate and local only as the title's tooltip (ADR-023).

### US-9 (P3): Near real time Claude Code limits while working

- **Given** the status line bridge is installed, **then** during an active Claude Code session the limits update after each assistant message without running extra probes.

### US-10 (P2): Troubleshoot

- A "Copy diagnostics" action copies app version, macOS version, Mac chip, per provider status (for Claude Code: detected `claude` path, version and executable type, last probe exit code, last raw `/usage` output) and the last error per source to the clipboard. It never includes tokens.

### US-11 (P1): Provider ready architecture (developer story)

*As the developer, I want to add a second AI coding tool later by writing one provider module, without touching Claude Code code, the scheduler, notifications or UI layout logic.*

- **Given** a `FakeProvider` in the test support target whose shape differs from Claude Code (only a weekly window, no cache token categories, limits only pushed and never polled), **then** the full pipeline works with it: scheduling, persistence, notification planning, section states and menu bar selection.
- **Given** the debug build flag `CONTRIBUSAGE_FAKE_PROVIDER`, **then** the app registers the fake provider next to Claude Code and the popover shows two provider groups correctly.
- The core target compiles without any provider target (verified by the package graph, [NFR-17](#6-non-functional-requirements)).

### US-12 (P2): Turn a provider off

- **Given** I disable a provider in Settings, **then** nothing runs for it (no processes, no file watching), its popover group disappears, and the menu bar falls back to the next available source.
- Its history is kept until I explicitly choose "Delete data for this provider" in Settings.
- GitHub can be switched off the same way, without disconnecting: the token stays in the Keychain (FR-43).

### US-13 (P2): Arrange the popover

- **Given** the Popover tab in Settings, **then** I can drag the blocks (each provider group, the heatmap, GitHub) into any order, show or hide each block, and show, hide and reorder a provider group's limits, activity and insights sections. The open popover follows at once.
- Hiding a section never silences the menu bar or notifications: their data keeps coming (FR-46).
- **Given** every block is hidden or every source is off, **then** the popover shows "Nothing to show" with "Open Settings…", and the menu bar shows only its icon.

### US-14 (P2): One heatmap for all my activity

- **Given** GitHub and Claude Code are both in the heatmap, **then** each day shows GitHub's level in green and Claude Code's in orange: as stripes of one cell ("Combined", the default) or as one grid per source sharing the week columns ("Stacked").
- Hovering a day lists every layer ("2026-09-27 · Claude Code: 1.2M tokens · GitHub: 5 contributions"); VoiceOver reads the same line, so no value depends on color alone.
- I can take a source out of the heatmap without hiding its other sections.

### US-15 (P3, after v1): See which plan each tool runs on

*Plans decide what a tool reports: a free plan can lack windows a paid plan has. Once a second provider exists (T-5.17), the popover must make clear why one group shows less than another.*

- **Given** a provider reports its plan, **then** its group header shows it next to the display name ("Claude Code · Max").
- **Given** a plan delivers fewer windows or capabilities, **then** the group shows only what the plan delivers, and the plan in the header explains the gap.
- Copy statistics and diagnostics include the plan.

---

## 5. Functional requirements

### 5.1 Provider framework

| ID | Pri | Requirement |
|---|---|---|
| FR-1 | P1 | **Registry.** The app registers providers at launch from a static list (v1: `claude-code`). Each provider supplies a `ProviderDescriptor` (10.2): ID, display name, SF Symbol, capabilities, token categories and schedule policies. Registration order defines display order. |
| FR-2 | P1 | **Enablement.** Each provider can be enabled or disabled in Settings. A disabled provider starts no processes, watches no files and schedules nothing; only the Providers tab, while shown, may detect its availability (ADR-028). Default on first run: enabled if its detection reports `available`. |
| FR-3 | P1 | **Detection.** Each provider reports its availability: `available(version)`, `notInstalled`, `notSignedIn`, `unsupportedPlan(note)` or `unknown`. Detection must be cheap and is re-run at most once per minute on popover open while not `available`. |
| FR-4 | P1 | **Popover groups.** The popover shows one group per enabled provider, in the block order and visibility of FR-44 (default: registry order), each with its capability sections (limits, activity, insights). With a single provider no provider picker or extra chrome is shown. |
| FR-5 | P1 | **Window kinds.** Every provider classifies each of its usage windows as `session`, `weekly` or `other`. Menu bar modes and settings use kinds. Labels are kept verbatim as the window's key; the UI shows the provider's window title for a label: its known labels translated, any other label as printed (ADR-033). |

### 5.2 Claude Code: plan limits

| ID | Pri | Requirement |
|---|---|---|
| FR-6 | P1 | **Locate `claude`.** Resolution order: (1) user override in Settings, (2) `command -v claude` run once through the user's login shell with `-lc` (usually `/bin/zsh`), (3) known locations on Linux: `~/.local/bin/claude` (native installer), `~/.npm-global/bin/claude`, `/usr/local/bin/claude`, `/usr/bin/claude`. A candidate is valid if `claude --version` exits with 0 within 10 s. Cache the resolved path and version; re-resolve when the cached path stops working. |
| FR-7 | P1 | **Probe.** Run `claude -p "/usage" --no-session-persistence` with the probe folder as working directory, stdin connected to `/dev/null`, stdout and stderr captured, 30 s timeout. At most one probe runs at a time (single flight: concurrent requests await the running probe). |
| FR-8 | P1 | **Parse limits.** Extract every usage window line generically (grammar in [8.1.3](#813-parsing-rules)). Keep label text exactly as printed so new windows appear without code changes. Also extract: subscription status line, insights block (structured, P-11), raw output. |
| FR-9 | P1 | **Unparseable output.** If a probe exits 0 but yields zero windows, the Claude Code limits state becomes `unparseable`, the previous snapshot is kept (marked stale), and the popover offers "Show raw output". |
| FR-10 | P1 | **Persist.** The last successful limits snapshot per provider is persisted and shown immediately on app launch (marked stale if older than the threshold). |
| FR-11 | P1 | **Reset handling.** When `now >= resetsAt` for a window, display it as "reset" (0 % is not assumed, the value is unknown until the next refresh) and schedule a refresh respecting the minimum interval. |

### 5.3 Menu bar

| ID | Pri | Requirement |
|---|---|---|
| FR-12 | P2 | **Menu bar label.** Display modes: `primary` (default: the `session` window of the menu bar provider), `weekly` (the first `weekly` window of the menu bar provider), `highest` (the window with the highest percentage across all enabled providers), `githubToday`, `primaryAndGitHub`, `iconOnly` (the `primary` meters without text, both rings in the `rings` style). The menu bar provider is a setting; it defaults to the first available provider and is hidden in Settings while only one provider exists. If the chosen window is missing, fall back to `highest`, then to `?`. Settings offers only the modes whose source is on (FR-2, FR-43); a saved mode that is not offered shows the first one that is, so with every source off the label is `iconOnly`. Hiding a section does not change the label (FR-46). |
| FR-51 | P2 | **Menu bar style and color.** The label is drawn in one of seven styles (ADR-035): `prompt` (default; the app's mark, a prompt `›` with three code lines that fill as the window is used), `rings` (the shown window outside, the same provider's other window of kind `session` or `weekly` inside), `ring` (one ring with the number inside and no text), `line` (a prompt and a cursor line that fills), `heatmap` (the label source's last three weeks in the levels of 11.4, rows are weeks from Sunday), `sharedHeatmap` (the same three weeks with the shared heatmap's layers, split into stripes as in FR-49's Combined style; the label's source takes the color, the others keep their hues; the label's source alone while no layer is in the heatmap) and `text` (the source's name and the value, for example "Claude Code 23%"). In the GitHub modes the meter is today's contribution level (FR-20) of four, and the `rings` style's inner ring the days with contributions in the current week (8.4.4's week) of seven; they show activity, not a limit, so they never turn red or orange. Its color is one of: the provider's heatmap hue (default; GitHub green for the GitHub modes), by usage (11.4's levels), the system accent color, monochrome, or a custom color. From 90 % the meter is red in every color but monochrome; the heatmap styles show activity, not the limit, so they keep their hues and the value turns red instead (red next to GitHub's green, 11.4); a stale value draws its meter at half strength. Settings' General tab shows each style as a live preview of the label. |

### 5.4 Notifications

| ID | Pri | Requirement |
|---|---|---|
| FR-13 | P2 | Notify when a window's percentage crosses a threshold upward. Defaults: 80 and 95. Configurable list of up to 3 values between 50 and 99, shared by all providers. When one refresh crosses several thresholds of a window, only the highest is notified. |
| FR-14 | P2 | Optional notification when a window that had crossed a threshold resets. Off by default. |
| FR-15 | P2 | De-duplication key: `providerID + label + resetsAt + threshold`; once a threshold is sent, every lower threshold of the same cycle counts as sent (ADR-027). Keys are persisted so an app restart does not repeat notifications. Keys older than 8 days are pruned. A cycle ends only when the reported reset time differs from the cycle's by more than 1 h, or when the window is reported without a reset time after the cycle's has passed; otherwise a missing reset time on either side keeps the cycle. The cycle keeps its first known reset time, which is the `resetsAt` of its key and its notifications' identifier (ADR-038). |
| FR-52 | P2 | **Delivery.** The notifications of one refresh are delivered one at a time, 3 s apart, in the order the provider reports its windows. All notifications of a provider share one thread (its provider ID), so Notification Center stacks them; each keeps its own identifier per window and cycle (ADR-038). |

### 5.5 GitHub

| ID | Pri | Requirement |
|---|---|---|
| FR-16 | P1 | Store the GitHub token only in KWallet (Secret Service API), network wallet, folder contribusage, key token. Never in settings, files or logs. |
| FR-17 | P1 | Validate a token by querying `viewer { login }`. Show the resolved login in Settings ("@login", "Connected"). |
| FR-18 | P1 | Fetch the contribution calendar for the last 365 days via GraphQL ([8.4](#84-github-contributions)). |
| FR-19 | P1 | Compute: today's count, this week's count (week starts per user locale), total for the range, current streak, longest streak within the range ([8.4.4](#844-statistics-rules)). |
| FR-20 | P1 | Render GitHub's `contributionLevel` (5 levels) as GitHub's layer of the shared heatmap (FR-48), using the app's own palette that adapts to light and dark mode. Range 26 weeks, not configurable. |
| FR-21 | P3 | Optional import of the token from the GitHub CLI (`gh auth token`), only after an explicit button press, showing which account will be used. |

### 5.6 Claude Code: activity

| ID | Pri | Requirement |
|---|---|---|
| FR-22 | P2 | Discover transcript roots: `$CLAUDE_CONFIG_DIR/projects` if set in the login shell environment, `~/.claude/projects`, `~/.config/claude/projects`. Scan `**/*.jsonl` recursively. |
| FR-23 | P2 | Decode lines leniently: unknown fields ignored, malformed lines skipped and counted (shown in diagnostics). |
| FR-24 | P2 | De-duplicate usage entries by `message.id + ":" + requestId` ([8.3.3](#833-aggregation-rules)). |
| FR-25 | P2 | Aggregate per local calendar day and per model: requests, sessions, input, output, cache write, cache read tokens. |
| FR-26 | P2 | Read incrementally: remember per file its identity, size and byte offset; only parse appended complete lines. Rescan a file whose identity changed or whose size shrank. |
| FR-27 | P2 | Watch transcript roots with FSEvents; debounce 5 s; process changes at utility QoS. |
| FR-28 | P2 | Ignore any transcript whose project directory corresponds to the probe folder (defensive, since probes should not persist sessions). |
| FR-29 | P2 | Keep a persisted daily history for 365 days, independent of Claude Code's own transcript cleanup ([8.3.4](#834-history-and-retention)). |

### 5.7 App shell

| ID | Pri | Requirement |
|---|---|---|
| FR-30 | P1 | Agent app: `LSUIElement = YES` (no Dock icon, no app switcher entry). |
| FR-31 | P1 | Popover layout: the visible blocks of FR-44 in their configured order (a provider group shows its sections in their order, default limits, activity, insights), then the footer. Each section renders its own loading, empty, error and stale states. |
| FR-32 | P1 | Manual refresh button in the footer (and `⌘R` while the popover is focused). Refreshes all sources of all enabled providers and GitHub, subject to minimum intervals ([section 12](#12-refresh-policy)). |
| FR-33 | P2 | Settings window (SwiftUI `Settings` scene) covering US-7 and US-12. |
| FR-34 | P2 | Launch at login via `SMAppService.mainApp`. |
| FR-35 | P1 | Quit item in the footer (`⌘Q`). |
| FR-36 | P2 | "Copy diagnostics" (US-10) in Settings' Advanced tab (ADR-030). For the resolved `claude` executable, diagnostics state whether it is a Mach-O arm64 binary, a Mach-O x86_64 binary (would need Rosetta) or a script (for example an npm shim). |
| FR-37 | P2 | ~~First run onboarding in the popover: detects providers, offers GitHub connection, asks notification permission, offers launch at login. Each step skippable.~~ Dropped (ADR-029): the popover's not configured states and Settings cover each step where it is needed. |

### 5.8 Optional features (P3)

| ID | Pri | Requirement |
|---|---|---|
| FR-38 | P3 | Insights area showing the Claude Code `/usage` "What's contributing" block, structured by period (US-8, ADR-023). Period labels, the request and session counts of a summary and the known sentences of the note show translated, shares and rankings as printed (ADR-033). Generic: any provider with the `insights` capability supplies an `Insights` value (10.3). |
| FR-39 | P3 | Claude Code status line bridge support: watch the bridge file, merge its windows with probe data (US-9, [8.2](#82-claude-code-status-line-bridge-optional)). Guided installer that backs up `~/.claude/settings.json`, never overwrites an existing `statusLine` without showing a diff and getting consent, and can uninstall cleanly. |
| FR-40 | P3 | "API-equivalent value" estimate from token counts using a user editable price table per provider, clearly labelled as an estimate, hidden by default. It covers the last 7 days: the tokens per category and the estimated money value, priced per model from `ActivityDay.byModel`; a model missing from the price table is counted in tokens and named as unpriced. |
| FR-41 | P3 | Support a custom Claude config directory chosen in Settings. |
| FR-42 | P2 | "Copy" button in the popover footer (copy statistics): copies what the popover shows as plain text, in its block order and only the visible sections (FR-44): per provider its limits, activity and, while shown, insights with every period; the heatmap one line per layer with its 26 week total; GitHub's statistics; a section without values its error message, without diagnostics (ADR-030). |
| FR-50 | P3 | **Plan (after v1).** A provider may report its plan: the tool's own name for it, verbatim (US-15). It comes only from an official interface named in the provider's section 8, never from credential files or Keychain items (section 2); Claude Code's source is open (R-6). The group header, copy statistics (FR-42) and diagnostics (FR-36) show it; without one the header stays as it is. Sections and windows still follow what the provider reports, so the app holds no per plan logic. A plan change raises no notification. Where the plan travels in the types (with the limits report or with detection) is decided with R-6. |

### 5.9 Popover layout and shared heatmap

| ID | Pri | Requirement |
|---|---|---|
| FR-43 | P2 | **GitHub switch.** GitHub can be switched on or off in Settings' GitHub tab; default on. Off: no requests, its block and heatmap layer are hidden and the GitHub menu bar modes are not offered; the token stays in the Keychain. "Disconnect" still deletes the token (11.6). |
| FR-44 | P2 | **Blocks.** The popover consists of blocks: one per enabled provider, the heatmap, GitHub. In Settings' Popover tab the user sets their order and shows or hides each block, and shows or hides each section of a provider group (limits, activity, insights, as far as its capabilities declare them), and drags those sections into any order within their group (ADR-034). Default: everything visible, provider groups in registry order, then the heatmap, then GitHub. The footer always comes last. |
| FR-45 | P2 | **Layout persistence.** The layout (order, section order per provider, hidden blocks and sections, heatmap membership, heatmap style) is saved with the other settings in UserDefaults. Blocks are keyed by provider ID, `heatmap` and `github`; these two IDs are reserved and no provider may use them. A block missing from the saved order (for example a newly registered provider) is appended at the end and shown. Saved keys of providers that are not registered are kept and ignored. |
| FR-46 | P2 | **Work follows use.** A source's data is fetched only while the source is on and something uses it: a visible section, its heatmap layer, the menu bar mode, or notifications. In v1: a provider's limits are fetched while it is on, because notifications use them; its activity is watched only while its activity section is visible, its heatmap layer is on or the menu bar draws it in the `heatmap` or `sharedHeatmap` style (FR-51); GitHub is fetched only while its block is visible, its layer is on, or the menu bar mode or the `sharedHeatmap` style shows it. A source nobody uses behaves like a disabled one (FR-2). |
| FR-47 | P2 | **Nothing to show.** While no visible block has a source that is on, the popover shows "Nothing to show" with "Open Settings…" above the footer. |
| FR-48 | P2 | **Shared heatmap.** The heatmap block shows the last 26 weeks, with one layer per source that is on and in the heatmap ("In heatmap" in the Popover tab, default on): GitHub's contributions, and for every provider with the `activity` capability its total tokens per day. Levels 0 to 4: GitHub's layer uses `contributionLevel` (FR-20); a provider's layer uses 0 for a day without tokens and otherwise the quartile of that day among the layer's non-zero days in the shown range. Days a source has no data for (before its history begins) are drawn like 0. Layers are never summed (7.7). The block is hidden while it has no layer (ADR-032). |
| FR-49 | P2 | **Heatmap styles.** "Combined" (default): one grid, each cell split into equal vertical stripes, one per layer with activity that day, in block order; a day with one active layer fills the cell with it, a day with none is level 0. "Stacked": one grid per layer in block order, sharing the week columns; there are no month labels. The style is chosen in the Popover tab, which notes that Stacked reads better from three layers on. A legend names each layer in its hue. Tooltip and VoiceOver read one line per day with every layer in block order: "2026-09-27 · Claude Code: 1.2M tokens · GitHub: 5 contributions"; a layer without data for that day reads "no data". |

---

## 6. Non-functional requirements

| ID | Category | Requirement | How it is verified |
|---|---|---|---|
| NFR-1 | CPU | Popover closed and no refresh running: average CPU below 0.1 % over 10 minutes. No app timer fires more often than once per minute while the popover is closed. | Activity Monitor, Instruments Time Profiler |
| NFR-2 | Memory | Typical resident memory below 80 MB with one provider; each additional provider adds at most 20 MB. | Activity Monitor after 24 h |
| NFR-3 | Latency | Popover shows cached data within 150 ms of the click. Opening the popover never waits for a network call or a process. | Manual, Instruments |
| NFR-4 | Main thread | App code never blocks the main thread longer than 16 ms. Process execution, file IO and parsing run off the main actor. | Thread Performance Checker, code review |
| NFR-5 | Probe budget | Automatic Claude Code probes every 5 min by default, never more often than every 1 min (ADR-039). Manual refresh at most every 30 s. | Unit tests on the scheduler |
| NFR-6 | GitHub budget | Automatic GitHub fetches at most every 30 min by default, never more often than every 10 min. Rate limit headers respected. | Unit tests |
| NFR-7 | Scan speed | Initial scan of 500 MB of Claude Code transcripts under 30 s on an Apple M1 (the slowest supported chip) at utility QoS; incremental update under 200 ms. | Performance test with generated fixtures; on an M3 against half the budget ([ADR-026](llm-wiki/decisions/0026-nfr-7-on-an-m3.md)) |
| NFR-8 | Accessibility | Every bar, chart and heatmap has a VoiceOver label; information is never conveyed by color alone; popover and Settings are fully keyboard navigable. | Accessibility Inspector, VoiceOver pass |
| NFR-9 | Appearance | Correct in light and dark mode; respects Reduce Motion and Reduce Transparency; uses system fonts and semantic colors. | Manual checklist |
| NFR-10 | Localization | All user facing strings in a String Catalog. English first; German ships as well, because an app without the user's language formats dates and units in English (ADR-031); a provider's known window labels are translated too (ADR-033). Dates, numbers and relative times formatted with the user's locale. | Build check (CI: catalogs match the extracted strings and carry German, ADR-031), pseudo-localization run |
| NFR-11 | Code quality | Swift 6 language mode, strict concurrency, zero compiler warnings. | CI build |
| NFR-12 | Resilience | Survives: `claude` missing or broken, logged out, offline, sleep and wake, time zone change, clock change, malformed or huge files, deleted transcript folders, a provider throwing on every call. | Test matrix in 16.5 |
| NFR-13 | Privacy | The app itself connects only to `api.github.com`. Logs never contain tokens. | Little Snitch or `nettop`, log review |
| NFR-14 | Energy | Periodic work uses `NSBackgroundActivityScheduler` or timers with at least 10 % tolerance; nothing runs while the Mac sleeps; Low Power Mode doubles all automatic intervals. | Energy tab in Activity Monitor |
| NFR-15 | Tests | Line coverage of at least 80 % for parsing, statistics and scheduling code in every package target. | `swift test --enable-code-coverage` |
| NFR-16 | Architecture | The shipped executable contains only an arm64 slice. `LSMinimumSystemVersion` is 14.0. | `lipo -archs` on the built executable prints `arm64` (CI check) |
| NFR-17 | Provider isolation | `ContribusageCore` has no dependency on any provider or on the GitHub target. Provider targets do not depend on each other. | `Package.swift` target graph; `ArchitectureTests` (run by `swift test` locally and in CI) fails when `ContribusageCore` imports any other target or another target imports anything but the core |
| NFR-18 | Process budget | Across all providers, at most one background child process started by the app runs at a time (global process single flight). | Unit tests on the coordinator |

---

## 8. Data sources

Sections 8.1 to 8.3 form the **Claude Code provider** (`claude-code`). Section 8.4 is GitHub, which is not a provider but a separate, always available source.

### 8.1 Claude Code: plan limits via `/usage` probe

Compliance note: see [2.1](#21-why-principle-2-matters-claude-code). The probe runs the official, unmodified `claude` binary with the user's own login, exactly as the user would in Terminal.

#### 8.1.1 Invocation

```text
executable:   <resolved claude path>                  (FR-6)
arguments:    -p "/usage" --no-session-persistence
cwd:          $XDG_DATA_HOME/contribusage/providers/claude-code/probe/   (empty directory, created on demand)
stdin:        /dev/null
stdout/err:   captured (stdout parsed, stderr kept for diagnostics only)
timeout:      30 s, then SIGTERM, then SIGKILL after 2 s
environment:  inherited app environment plus PATH from the login shell (see FR-6)
```

Why each piece matters:

- `--no-session-persistence` stops the probe from being saved as a session on disk, so probes do not appear in `claude --resume` and do not inflate activity statistics.
- The empty probe folder matters because print mode skips the workspace trust dialog, and the CLI's own help advises using `-p` only in trusted directories. An empty folder guarantees that no project settings, hooks or MCP configuration from some random directory are executed.
- `stdin` must be `/dev/null` because print mode reads piped input and could otherwise wait.

#### 8.1.2 Expected output

A real sample (September 2026, subscription plan) is in `tests/claudecode/fixtures/usage/`. Structure:

1. A status line about how Claude Code is billed ("You are currently using your subscription…").
2. One line per usage window: `<label>: <n>% used · resets <when> (<IANA time zone>)`.
3. Optionally a block starting with "What's contributing to your limits usage?" with approximate, local only statistics.

#### 8.1.3 Parsing rules

| Rule | Detail |
|---|---|
| P-1 | Strip ANSI escape sequences (`ESC [ … letter`) before anything else. |
| P-2 | Split into lines; each line is tested independently. |
| P-3 | Window line grammar (Swift Regex, whole line): `\s*(?<label>[^:]+):\s*<?\s*(?<pct>\d+(?:\.\d+)?)%\s*used(?:.*?\bresets\s+(?<reset>.+?))?\s*` |
| P-4 | Label is trimmed and kept verbatim. Known labels today: `Current session`, `Current week (all models)`. Model specific weekly windows may appear on some plans (for example `Current week (<model>)`). Parsing keeps every form; the popup shows one week bar: `Current week (all models)` reads `Current week`, and a model's own week window is hidden (ADR-042). |
| P-5 | `<1%` is parsed as 0.5 and displayed as `<1%` (keep a flag `isBelowOne`). |
| P-6 | Reset clause: optional trailing `(<IANA zone>)`; if the identifier is unknown, use the Mac's time zone. |
| P-7 | Reset formats tried in order: `MMM d 'at' h:mm a`, `MMM d 'at' h a`, `MMM d, h:mm a`, `MMM d, h a`, `MMM d`, `h:mm a`, `h a` (after normalizing `am`/`pm` to `AM`/`PM`, English month names). The comma forms appear from Claude Code 2.1.289 ("resets Oct 5, 6am"; fixture `subscription-comma-dates.txt`). |
| P-8 | Year inference: the output has no year. Use the current year; if the result lies more than 24 h in the past, use next year. Time only formats resolve to the next occurrence after `now`. |
| P-9 | Unparseable reset clause: window still valid, `resetsAt = nil`, UI shows "reset time unknown". |
| P-10 | Billing status: the first non-empty line is stored as `billingNote`. If it does not mention "subscription", `fetch()` throws `SourceError.unsupportedPlan` (ADR-017) and the limits section shows "Plan limits are only available when Claude Code uses a Claude subscription". R-2 (Claude Code 2.1.284): API key billing and a logged out CLI both exit 0 and print the same cost summary, first line `Total cost:            $0.0000`, no windows (fixtures `not-subscription.txt`, `logged-out.txt`). |
| P-11 | Insights block: everything after the line starting with "What's contributing". An unindented line containing ` · ` opens a period (`label · summary`); indented lines fill it: `Top <x>: <name> <n>%, …` is a ranking titled `<X>`, `<n>% of your usage [was \| came from] <what>` a share labelled `<What>`, any other line a share with the whole line and no percent. Remaining unindented lines form the note. Nothing is dropped; a block without a period gives `insights = nil` (FR-38, ADR-023). |

P-1 to P-11 are implemented by `UsageParser` in `ContribusageClaudeCode/Limits/` (tasks T-2.1, T-2.2); its one entry point `UsageParser.report` returns the `LimitsReport`, and the provider decides P-10's `unsupportedPlan` from its `billingNote` (T-2.5).

#### 8.1.4 Window classification and token categories

| Label (after trimming) | Window kind |
|---|---|
| `Current session` | `session` |
| starts with `Current week` | `weekly` |
| anything else | `other` |

Classification is a pure function in the Claude Code target with its own tests. An unknown label is never dropped; it is shown as printed, as kind `other`.

Token categories reported by Claude Code: `input`, `output`, `cacheWrite`, `cacheRead` (all four).

Schedule policy for the probe: default 5 min, minimum 1 min, maximum 60 min (ADR-039), stale after 30 min, manual floor 30 s ([section 12](#12-refresh-policy)).

#### 8.1.5 Limitations (documented, accepted)

- The `/usage` text is written for humans and is not a documented, stable format. Claude Code updates frequently. The parser is generic and fixture tested so a wording change is detected immediately (principles 4 and 6).
- Values are as fresh as the last probe. Usage from claude.ai, Claude Desktop or other devices shares the same plan limits and is reflected by the next probe.
- A probe consumes no plan quota (R-1): `/usage` is a local command with no model turn. Each probe costs about 1.7 s of CPU in the `claude` process, which is why the default is 5 min and 1 min is a choice (ADR-039).

### 8.2 Claude Code: status line bridge (optional)

Claude Code runs a user configured `statusLine` command and passes session JSON on stdin. For Pro and Max subscribers this JSON contains, after the first API response of a session:

```json
"rate_limits": {
  "five_hour": { "used_percentage": 23.5, "resets_at": 1738425600 },
  "seven_day": { "used_percentage": 41.2, "resets_at": 1738857600 }
}
```

`resets_at` is Unix epoch seconds. Each window may be absent, and Claude Code drops a window once its reset time passes. Documentation: <https://code.claude.com/docs/en/statusline>

**Bridge contract**

- Script: in the `macos-reference` tag's SPEC.md (appendix D). Writes only when `rate_limits` is present, atomically (temp file plus `mv`).
- Output file: `~/Library/Application Support/contribusage/providers/claude-code/statusline-limits.json`

  ```json
  { "rate_limits": { "five_hour": { "used_percentage": 23.5, "resets_at": 1738425600 },
                     "seven_day": { "used_percentage": 41.2, "resets_at": 1738857600 } },
    "written_at": 1738420000.123 }
  ```

- Mapping: `five_hour` → label `Current session` (kind `session`); `seven_day` → label `Current week (all models)` (kind `weekly`).
- Merge rule: per label, the observation with the newest timestamp wins (`written_at` for the bridge, `fetchedAt` for probes). The bridge never removes windows that only the probe knows.
- A bridge file older than the staleness threshold is ignored.
- The bridge is delivered to the core as pushed `LimitsReport` updates of the Claude Code `LimitsSource`; the core does not know it exists.

### 8.3 Claude Code: activity from local transcripts

> ⚠️ The transcript format is **not a documented interface**. This section was verified against real files in research R-3; re-verify it when Claude Code changes.

#### 8.3.1 Location

`<root>/projects/<encoded project path>/<session id>.jsonl`, possibly with nested folders (for example for subagents). Roots per FR-22. The encoded project path is the absolute working directory with every character outside `[a-zA-Z0-9]` replaced by `-` (R-3 observed it for `/` and spaces).

#### 8.3.2 Fields used

| JSON path | Type | Use |
|---|---|---|
| `type` | string | Only `"assistant"` lines carry usage. |
| `timestamp` | ISO 8601 string | Day bucketing (converted to local time). |
| `sessionId` | string | Session counting. |
| `requestId` | string | De-duplication. |
| `message.id` | string | De-duplication. |
| `message.model` | string | Per model breakdown. Skip placeholder models such as `<synthetic>`; R-3 found them carrying zero usage. |
| `message.usage.input_tokens` | int | Input tokens. |
| `message.usage.output_tokens` | int | Output tokens. |
| `message.usage.cache_creation_input_tokens` | int | Cache write tokens. |
| `message.usage.cache_read_input_tokens` | int | Cache read tokens. |
| `isSidechain` | bool | Subagent traffic flag (for the optional "subagent share"). |

An illustrative line is in the `macos-reference` tag's SPEC.md (appendix E).

#### 8.3.3 Aggregation rules

- One API response can be written as several lines (one per content block) that repeat the same `usage`. Count each `message.id + ":" + requestId` exactly once. If `requestId` is missing, use `message.id` alone. If both are missing, count the line (and increment a diagnostics counter).
- A **request** is one unique key. A **session** is one unique `sessionId` per day.
- Day = local calendar day of `timestamp` in the Mac's current time zone.
- Totals per day and per model: `requests, sessions, inputTokens, outputTokens, cacheWriteTokens, cacheReadTokens`.

#### 8.3.4 History and retention

Claude Code removes old transcripts after a retention period (setting `cleanupPeriodDays`, default 30 days per Claude Code's settings documentation). The provider therefore keeps two layers:

1. **Live index** (in memory, rebuilt from the transcripts at launch, [ADR-024](llm-wiki/decisions/0024-live-index-in-memory.md)): unique keys and their usage for all entries in files that currently exist. Recent days are computed from it.
2. **History store** (persisted): one aggregate per day. A day is **frozen** into the history store once it is more than 48 h in the past. Frozen days are never recomputed from files, so deleted transcripts do not erase history. Keep 365 days.

Display rule: days younger than 48 h come from the live index, older days from the history store.

The history store implementation is generic (core), keyed by provider ID, so future providers with the `activity` capability reuse it.

#### 8.3.5 Incremental reading

Per file keep: `path, fileResourceIdentifier (or inode), size, offset, pendingPartialLine`.

- Size grew and identity unchanged: read from `offset`, split on `\n`, keep an incomplete last line as `pendingPartialLine`.
- Identity changed or size shrank: drop the file's entries from the live index and rescan it from 0.
- File deleted: drop from live index (frozen history is unaffected).

The incremental JSONL reader is generic (core); the Claude Code target supplies only the line decoder and aggregation keys.

#### 8.3.6 Validation

`ccusage` (`npx ccusage daily --json`) serves as an external reference implementation. Token totals per day must match within 1 % (R-3, T-4.7).

Checked 2026-10-02 (T-4.7): the app's daily token totals, compared by hand with `ccusage daily` on the same machine and dates, match within 1 %.

### 8.4 GitHub contributions

#### 8.4.1 Request

```text
POST https://api.github.com/graphql
Authorization: Bearer <token>
User-Agent: contribusage/<version>
Content-Type: application/json
```

Query and a sample response: appendix C of the `macos-reference` tag's SPEC.md. Variables: `from` = start of the local day 364 days ago, `to` = now (the API accepts a span of at most one year).

#### 8.4.2 Token

- Recommended: fine grained personal access token, read only, shortest practical expiry, no write permissions.
- Whether private contributions are counted depends on token type and profile settings; research R-4 decides the documented recommendation. Fallback: classic token with `read:user` only.
- Optional: import from `gh auth token` (FR-21).

#### 8.4.3 Rate limits and errors

- GraphQL has a points based hourly limit (5,000 points per hour for a user token); this query costs about 1 point.
- Read `x-ratelimit-remaining` and `x-ratelimit-reset`. A failed response (HTTP 200 with `errors`, 403 or 429) with remaining 0 is `rateLimited(until:)` the reset time, and GitHub fetches pause until then. GitHub reports an exhausted primary limit as HTTP 200 with an error and remaining 0; a successful response that spends the last point is used as is. Other 403 or 429 responses (secondary limits) are `http(status:)` and fall under the backoff, which waits longer than GitHub's one minute minimum (ADR-020).
- 401: token invalid, state `unauthorized`, no automatic retries until the token changes.
- GraphQL `errors` array with HTTP 200: treat as failure, keep previous snapshot.

#### 8.4.4 Statistics rules

- **Today** = the calendar day entry whose `date` equals today's date, 0 when there is none yet. The statistics take a `Calendar`: its time zone sets the day boundary (which zone: R-4; until then the app passes the Mac's, `Calendar.current`, ADR-022), its first weekday the week start (ADR-021). Entries after today (GitHub's day ahead of the local one) count toward no statistic.
- **Current streak** = number of consecutive days with `count > 0` ending today; if today is 0 so far, the streak ending yesterday is shown and, when it is not 0, marked "extend today".
- **Longest streak** = longest run within the fetched range, up to today.
- **This week** = sum from the locale's first weekday to today.
- **Total** = `totalContributions` from the API (not recomputed).

---

## 11. UI specification

### 11.1 Menu bar item

- One image drawn by the app in the style and color of FR-51, not a template (ADR-035). Its drawing handler runs whenever the status item draws, in the status item's appearance, so the text and the neutral parts use `labelColor` and follow a light or dark menu bar; the meter keeps its color.
- Content by mode (FR-12), for example `primary` in the `prompt` style: the mark with the session window's fill, then `23%`.
- Stale value: the same text (`23%`) and the meter at half strength, with no text marker (ADR-049). Unknown: `?` and an empty meter. At 90 % or more the meter is red (11.4) unless the color is monochrome.
- With more than one enabled provider and mode `highest` (or a fallback to it), the provider's symbol stands before the glyph.
- `githubToday`: today's count (`7`) with the glyph filled to today's contribution level (FR-51), or GitHub's last three weeks in the `heatmap` style; `primaryAndGitHub`: the primary label followed by ` · 7`. GitHub without data yet reads `?`.
- Width stays stable and the label ends at its value: monospaced digits, with figure spaces in front up to two digits; `100%` widens the label by one digit (ADR-035).
- VoiceOver reads the source's name and the text ("Claude Code 23%").

### 11.2 Popover (width 360 pt)

```text
┌────────────────────────────────────────────┐
│ ◆ Claude Code                  updated 2m  │
│ Current session                       23%  │
│ ██████░░░░░░░░░░░░░░░░░░  resets in 2h 10m │
│ Current week                          41%  │
│ ██████████░░░░░░░░░░░░░░  resets Sun 18:59 │
│ On this Mac                                │
│ Today: 668 requests · 8 sessions · 4.2M    │
│ ▁ ▃ ▂ ▅ ▇ ▆ ▃   last 7 days                │
│ ▸ Insights                                 │
├────────────────────────────────────────────┤
│ Heatmap             ■ Claude Code ■ GitHub │
│ ▚▞▚█░▞▚▒▞▚█▞▚░▞ … (26 weeks × 7 days)       │
├────────────────────────────────────────────┤
│ GitHub  @login                             │
│ Today 5 · Streak 12 days · Year 1,234      │
├────────────────────────────────────────────┤
│ ⟳             ⚙︎             ⧉             ⏻ │
└────────────────────────────────────────────┘
```

- Each provider group has a header with its symbol, display name and the age of its limits data. Additional providers appear as further groups; all blocks follow the order and visibility of FR-44, separated by dividers. The sketch shows the default order and the Combined heatmap style (FR-49).
- Countdowns and ages update once per minute, only while the popover is visible (`TimelineView(.periodic(from: .now, by: 60))`).
- Hovering a heatmap cell shows its FR-49 line ("2026-09-27 · Claude Code: 1.2M tokens · GitHub: 5 contributions"). Hovering a reset text shows the absolute local date and time. Both are tooltips that appear after 0.2 s.
- The 7 day chart uses Swift Charts, total tokens per day, with an accessibility summary.
- If the popover content exceeds the screen height (many providers), the provider groups scroll; the footer stays pinned.
- The footer's buttons show only their icons, with the titles as tooltips and VoiceOver labels: the titles do not fit 360 pt in every language (German).
- The popover paints no background of its own: the menu bar window's Liquid Glass follows System Settings → Appearance (clear or tinted glass) and Reduce Transparency, and the content uses semantic, vibrant styles on top of it.

### 11.3 Section states

| State | Limits | Activity | GitHub |
|---|---|---|---|
| Provider disabled | Group hidden | Group hidden | n/a |
| Hidden by the user (FR-44), GitHub off (FR-43) | Section hidden | Section hidden | Block hidden |
| Not configured | "Claude Code not found" + "Locate…" button | "No Claude Code sessions found on this Mac" | token form: field, Connect, link to create a token (ADR-043) |
| First load | Skeleton bars | Skeleton rows | Skeleton grid |
| Loaded | Values | Values | Values |
| Stale | Values dimmed + "updated 42 min ago" | n/a | Values dimmed + age |
| Failed, has previous | Previous values dimmed + one line error + "Retry" | same | same |
| Failed, no previous | Error message + action | same | same |
| Unsupported plan | "Plan limits need a Claude subscription login in Claude Code" | unaffected | unaffected |
| Unparseable | Previous values dimmed + "Couldn't read /usage output" + "Show raw output" | n/a | n/a |

The texts in this table show Claude Code as the example; the app words each state once with the provider's display name, so providers supply no strings, and the unsupported plan text is the tool's own note (ADR-017, ADR-031).

The heatmap block takes each layer from its source's last snapshot, stale or failed ones included, and dims a stale layer like its section. A layer without any snapshot counts every day as "no data".

### 11.4 Colors and thresholds

| Level | Range | Bar color |
|---|---|---|
| Normal | below 70 % | Accent color |
| Warning | 70 to 89 % | System yellow/orange |
| Critical | 90 % and above | System red |

The percentage is always shown as text next to the bar. Each heatmap layer draws levels 1 to 4 in four steps of its hue (40 to 100 % opacity, T-5.7) and level 0 in a neutral secondary fill, on light and dark backgrounds. GitHub's hue is system green; a provider's is its descriptor's `heatmapHue` (Claude Code: system orange). Red is not a layer hue: it means critical here, and red next to green fails the most common color blindness (ADR-032).

### 11.5 Formatting

- Percent: integers; `<1%` when `isBelowOne`.
- Tokens: compact notation with the user's locale (`4.2M`, `812K`).
- Reset text: more than 24 h away → weekday and time ("resets Sun 18:59"); 1 to 24 h → "resets in 2 h 10 min"; under 1 h → "resets in 12 min", never below 1 min; passed → "reset, refreshing…" (US-2) with the percentage shown as "–" (FR-11). Durations use the locale's condensed abbreviated units (en_US: "2 hr 10 min"); VoiceOver gets wide units ("2 hours, 10 minutes").
- Ages: "updated just now", "updated 2 min ago", "updated 3 h ago".

### 11.6 Settings window

| Tab | Controls |
|---|---|
| General | Menu bar display mode, offering only modes whose source is on (FR-12), and menu bar provider (hidden while only one provider exists), both with T-5.10 (ADR-028); menu bar style as live previews and color, with a color well for the custom one (FR-51); launch at login; notification thresholds; notify on reset |
| Popover | Block list in popover order, reorderable by drag (FR-44); per block a visibility toggle and, for a source with daily data, "In heatmap" (FR-48); a provider row expands into toggles for the sections its capabilities declare, reorderable by drag within the row (ADR-034); a drop target lights up and rows slide into place, without motion under Reduce Motion; a block whose source is off is greyed out with a note pointing to Providers or GitHub; heatmap style Combined or Stacked with a legend preview and the three-layer hint (FR-49) |
| Providers | List of registered providers with enable toggle and availability status. Selecting Claude Code shows: detected `claude` path, version and executable type; override path (file picker) and "Test" button; probe interval (1 to 60 min); status line bridge instructions (P3: installer); "Delete data for this provider", offered while the provider is off (ADR-028) |
| GitHub | On/off switch, keeping the token (FR-43); account row: "@login" with "Connected" or, after a 401, "Token invalid or expired", and "Disconnect" (deletes the token); token secure field with "Connect", or "Replace" while connected (the saved token stays until the new one validates); refresh interval (10 min to 6 h); link to GitHub's token creation page |
| Advanced | Open data folder; reset caches (never history, never the notification keys, ADR-028); copy diagnostics |
| About | The app icon, name, version and build, a one-line description, the copyright (author, from the Info.plist), links to the source code, the license and the README's acknowledgements |

### 11.7 Accessibility examples

- Limits bar: "Claude Code, Current session, 23 percent used, resets in 2 hours 10 minutes."
- Heatmap: container label "Heatmap, last 26 weeks, Claude Code 41M tokens, GitHub 812 contributions"; individual days reachable by keyboard, each read as its FR-49 line. GitHub's statistics: "GitHub, today 5, current streak 12 days, last year 1,234".
- Chart: "Claude Code tokens per day, last 7 days, highest Thursday with 5.1 million."

---

## 12. Refresh policy

| Source | Automatic interval (default / min / max) | Extra triggers | Stale after | On failure |
|---|---|---|---|---|
| Claude Code limits probe | 5 min / 1 min / 60 min | Popover opened and data older than 5 min; a window's reset time reached; wake (after 10 s); manual | 30 min | Exponential backoff: 2×, 4×, 8× the interval, capped at 60 min; reset on success |
| Claude Code bridge file | Event driven (file watch) | none | 30 min | Ignore file, fall back to probe |
| Claude Code transcripts | Event driven (FSEvents, 5 s debounce) | Popover opened; wake | not applicable | Retry on next event |
| GitHub | 30 min / 10 min / 6 h | Popover opened and data older than 10 min; wake (after 10 s); token changed; manual | 2 h | Rate limit: run again at the reset, not earlier even manually; other errors: 2×, 4×, 8× backoff capped at 6 h; no token or 401: no automatic run until the token changes, manual refresh still runs (ADR-022) |
| Future providers | From their `SchedulePolicy` | Same triggers as above for polled limits | From policy | Same backoff rules |

Global rules:

1. Nothing runs while the Mac sleeps. On wake, wait 10 s (network), then run every source that is due.
2. When offline (`NWPathMonitor`), network dependent sources (the Claude Code probe, GitHub) are skipped and marked "offline"; they run as soon as the path is satisfied, within their `manualFloor`. A source waiting for a token or a rate limit's reset keeps that state and that wait. Each provider declares in its descriptor section whether its polled source needs the network (Claude Code: yes).
3. Low Power Mode doubles automatic intervals.
4. Manual refresh ignores intervals and backoff, but never runs a polled source more often than its `manualFloor` (Claude Code probe and GitHub: 30 s).
5. At most one child process runs at a time across all providers (NFR-18). When several are due, they run in registry order.
6. Disabled providers are never scheduled.
7. The scheduling decision is a pure function, `nextRun(policy:lastSuccess:lastAttempt:failures:now:conditions:manual:triggers:) -> Date?`, fully unit tested and provider neutral. `conditions` carries online, Low Power Mode, asleep and the last wake; `nil` means not scheduled (asleep, or offline for a source that needs the network). The coordinator runs the unsupported-plan recheck (section 13) through it as a 6 h policy (ADR-018).
8. An extra trigger ("popover opened", "a window's reset time reached", and the last wake plus 10 s from `conditions`) is a date in `triggers`: the first one after the last run runs the source then, or once the minimum interval since its last run has passed, skipping interval and backoff. The coordinator passes its snapshot's `resetsAt` dates; the app reports the popover opening, which only the pass it starts sees, so a trigger not due at that moment is dropped (ADR-019).
9. Pushed limits are not scheduled: the coordinator consumes every enabled provider's `pushedUpdates()` while it runs and treats each report as a success (shown with origin `push`, persisted, planned for notifications). A push also counts as the polled source's last success. `start()` starts and stops the consumers when providers are enabled or disabled (ADR-036).
10. A polled source that runs the tool's own CLI or reads local files may run every minute (ADR-039); a source behind a network API keeps the budget its service allows (GitHub: NFR-6).

---

## 13. Error handling

| Condition | Detection | State | User facing message | Recovery |
|---|---|---|---|---|
| `claude` not found | FR-6 resolution fails | `notConfigured(.toolNotInstalled)` | "Claude Code wasn't found. Install it or locate it in Settings." | Re-resolve on each popover open (max once per minute) |
| Not logged in | Non-zero exit and the output says "login", "log in" or "logged in" (other non-zero exits: `processFailed` with the last 500 characters of stderr). R-2: Claude Code 2.1.284 prints no login text; logged out looks like API key billing and lands in the next row | `failed(.notLoggedIn)` | "Claude Code isn't logged in. Run `claude` in Terminal and log in." | Normal schedule |
| API key billing, no subscription | P-10; `fetch()` throws `SourceError.unsupportedPlan` | `notConfigured(.unsupportedPlan)` | See 11.3 | Re-check every 6 h |
| Probe timeout | 30 s elapsed | `failed(.timedOut)` | "Claude Code didn't answer in time." | Backoff |
| Unparseable output | Exit 0, zero windows | `failed(.unparseable)` | "Couldn't read the /usage output. Claude Code may have changed its format." | Keep previous; offer raw output; diagnostics in Settings (FR-36) |
| Offline | `NWPathMonitor` | `failed(.offline)` with previous | "Offline" badge | Auto on reconnect |
| GitHub 401 | HTTP status | `failed(.unauthorized)` | "GitHub token is invalid or expired." + "Change token…" button (opens Settings) | Stop until token changes |
| GitHub rate limited | Failed response with `x-ratelimit-remaining: 0` (8.4.3) | `failed(.rateLimited(until:))` | "GitHub rate limit, retrying at 15:04." | Wait until reset |
| Transcript root missing | Directory absent | `notConfigured(.noLocalData)` | See 11.3 | Watch parent directory for creation |
| Malformed transcript lines | Decode failure | Loaded, `skippedLines > 0` | Only in diagnostics | none |
| Provider throws unexpectedly | Any error not mapped | `failed(.providerSpecific)` for that provider only | "Something went wrong with <provider>." (diagnostics in Settings, FR-36) | Backoff; other providers unaffected |
| Disk write failure | Throwing write | Keep in memory | Only in diagnostics (log error) | Retry on next change |

---

## 14. Security and privacy

- **Secrets:** only the GitHub token, stored in the Keychain (`kSecClassGenericPassword`, accessible after first unlock, this device only). Never logged, never shown after saving (only "Connected as @login").
- **Tool credentials:** never accessed, for any provider (principle 2). Code review checklist item for every PR touching a provider target.
- **Process execution:** only executables resolved by a provider's locator (v1: the resolved `claude` path) and, once per provider for PATH resolution, `/bin/zsh -lc 'command -v <constant tool name>'` with a constant string. All other invocations use argument arrays; never build shell strings from user input or file content.
- **Network:** the app itself only talks to `api.github.com` over HTTPS. Provider targets have no network access by design (no `HTTPTransport` is passed to them). The `claude` child process talks to Anthropic as it always does; the app does not inspect or proxy that traffic.
- **Files:** reads only the roots declared in provider sections (v1: Claude Code transcript roots and the bridge file) and its own Application Support folder. Writes only its own folder (and, for FR-39 with consent, `~/.claude/settings.json` plus a backup).
- **Logging:** `os.Logger` with subsystem = bundle id and categories `providers.<id>`, `github`, `scheduler`, `persistence`. Paths logged with `privacy: .private`. No token, no transcript content, no prompt text.
- **Transcript content:** only the fields listed in 8.3.2 are decoded; message text is never decoded into memory structures beyond what the JSON decoder must skip.
- **Hardened Runtime:** on. **App Sandbox:** off ([ADR-006](llm-wiki/decisions/0006-no-app-sandbox.md)).

---

## 19. Open questions and risks

### 19.1 Open questions

| ID | Question | Blocks | Status |
|---|---|---|---|
| R-1 | Does a `/usage` probe consume plan quota? | Final NFR-5 values; the default interval (T-5.21) | Answered: no; default 5 min, minimum 1 min |
| R-2 | Exact Claude Code outputs for logged out and API key billing | T-2.2, section 13 | Answered |
| R-3 | Transcript format details and retention default | Phase 4 | Answered (8.3) |
| R-4 | GitHub private contributions and day boundaries | 8.4.2; the calendar passed in T-3.6 | Open |
| R-5 | Probe duration, child processes, MCP skipping, native execution | 8.1.1 | Open |
| Q-6 | Availability of the name "contribusage" and final icon | T-5.18 (name), T-5.23 (icon) | Answered: the name is free (T-5.18); the icon is done (T-5.23, ADR-040) |
| Q-7 | Which AI coding tool becomes the second provider | T-5.17 | Answered: none for v1; Codex CLI after v1 (ADR-037) |
| R-6 | Which official interface reports a tool's plan; for Claude Code, whether `claude auth status` names it and what else it prints | T-6.10 | Open |
| Q-8 | Could Claude Code's `stats-cache.json` (behind `/stats`) fill heatmap days that Claude Code already deleted from its transcripts? It is undocumented, versioned and updated with a lag; not used (ADR-032) | Nothing | Open |

### 19.2 Risks

| Risk | Likelihood | Impact | Mitigation |
|---|---|---|---|
| `/usage` text format changes | High | Claude Code limits unreadable until fixed | Generic parser, fixtures per version (16.6), raw output fallback, diagnostics |
| `-p "/usage"` stops working in print mode | Low | Primary Claude Code source lost | Status line bridge as fallback source (FR-39) |
| Anthropic changes its policy on automated CLI use | Low | Feature must be removed | Monitor the legal and compliance page; the app only runs the official binary as the user |
| Transcript format changes | Medium | Wrong activity numbers | Lenient decoder, ccusage comparison, skipped line counter |
| Probe is slow or heavy | Medium | Battery, CPU | R-5, conservative intervals, MCP skipping, Low Power Mode |
| `claude` path changes after updates | Medium | Probe fails | Re-resolution in FR-6 |
| Provider abstraction shaped too closely to Claude Code | Medium | Second provider needs core changes | `FakeProvider` with a deliberately different shape, conformance suite, `providerSpecific` error escape hatch |
| A future tool offers no compliant interface | Medium | That tool cannot be integrated (or only partly) | Provider gate (2.4); partial capabilities are acceptable, non compliant ones are not built |
| Users on Intel Macs ask for support | Low | Support requests | Documented non-goal (ADR-009); the README states "Apple Silicon only" |

---
