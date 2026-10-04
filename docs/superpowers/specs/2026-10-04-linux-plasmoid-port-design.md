# Linux port: contribusage as a KDE Plasma widget

Date: 2026-10-04 · Status: approved design, awaiting the implementation plan

## Goal

Port contribusage to Arch Linux on KDE Plasma 6 (Wayland) as a native app that uses as few system resources as possible. The base version looks and behaves like the macOS app; on top of that, its appearance is customizable.

## Decisions

| Question | Decision |
|---|---|
| Relation to the macOS app | This fork becomes a Linux-only app. macOS is not maintained here; the Swift code stays in git history as the reference implementation. |
| Desktop surface | A Plasma widget (plasmoid) in the panel: the drawn label in the panel, a popup on click. KDE only. |
| Architecture | Approach 1 of three: a C++/Qt logic library exposed as a QML module, loaded by the plasmoid inside plasmashell. Chosen because it uses the least system resources: no extra process, no IPC, no second runtime. |
| Rejected | A Rust daemon with a QML plasmoid over D-Bus (a second process and a D-Bus API to maintain). A Swift daemon reusing `ContribusageKit` (about 1 GB of toolchain, about 30 MB of runtime, Foundation gaps on Linux, no mature D-Bus library). |
| Customization | All four layers: theme colors and fonts, a toggle to follow the Plasma theme, layout density and sizes, and user QML overrides. |

The biggest resource cost is the `claude -p /usage` probe itself (about 1.7 s of CPU per run, see `llm-wiki/research/r-1-probe-cost.md`), not the architecture. The probe interval keeps its current default of 5 min and minimum of 1 min (ADR-039).

## 1. Architecture and components

```
plasmashell (already running)
└── contribusage plasmoid
    ├── QML UI            panel label (compact representation), popup (full representation), config pages
    └── C++ QML module    the applet's own module, built with plasma_add_applet (id org.contribusage.widget),
        │                 linking a static logic library
        ├── core/         models, schedule, refresh coordinator, notification planner,
        │                 persistence (versioned atomic JSON), activity index
        ├── claudecode/   /usage probe and parser, transcript activity, insights
        ├── github/       GraphQL client, token validation, contribution statistics
        └── platform/     seams and their live implementations
```

- **Boundaries as in the Swift code.** `core/` is provider-neutral and includes neither `claudecode/` nor `github/`. Each provider lives in its own directory. Every OS touchpoint goes through an abstract seam with a fake for tests, as `ContribusageTestSupport` does today (SPEC §10.6).
- **Seams and their live implementations:**

  | Seam | Swift today | Linux |
  |---|---|---|
  | `ProcessRunning` | Foundation `Process` | `QProcess` |
  | `FileEvents` | FSEvents | `QFileSystemWatcher` (inotify), debounced |
  | `FileReading` | `LiveFileReader`, `Darwin.read` | `QFile`, POSIX `read` |
  | `SecretStore` | Keychain | KWallet over the Secret Service API |
  | `HTTPTransport` | `URLSession` | `QNetworkAccessManager` |
  | `TimeSource` | system clock | `QDateTime` / `std::chrono` |
  | Notification delivery | `UNUserNotificationCenter` (app) | `KNotification` |
  | Conditions (network, sleep, power) | `NWPathMonitor`, `NSWorkspace`, low power | `QNetworkInformation`, logind `PrepareForSleep`, UPower `OnBattery` over D-Bus |
  | Launch at login | `SMAppService` | not needed: the widget starts with plasmashell |
  | Logging | `os.Logger` | `QLoggingCategory` |

- **Async only.** Nothing blocks the plasmashell GUI thread: the probe runs through `QProcess` signals, HTTP through `QNetworkAccessManager`, and the transcript scan through `QtConcurrent` on a worker thread.
- **QML sees view models only.** A few `QObject` view models with properties and signals take the place of `AppState`. QML holds views and wiring, no logic, as the app target does today.
- **Build:** CMake with KDE Extra CMake Modules, C++23 (for `std::expected`), `-Werror`, clang-format. Installed through a PKGBUILD.
- **Rules carried over from SPEC §2 and AGENTS.md:** leave Claude Code's credentials untouched; call no vendor backend; run `claude` only with the arguments in SPEC §8.1.1 and FR-6; keep the GitHub token only in the secret store and never in logs; zero warnings; fix a failing test in the code.

## 2. Data flow and persistence

- **Refresh loop:** the shape of ADR-018 and ADR-019. One `RefreshCoordinator` owns a single timer. `Schedule.nextRun` decides which job is due next: the Claude Code probe, the transcript scan or GitHub. Opening the popup, a reset time, waking from sleep and the network coming back are extra triggers. On battery it backs off as the current power rule does. Each pass merges its results into the in-memory state, persists them, and emits change signals so QML redraws only what changed.
- **One coordinator per process.** It is a process-wide singleton, so two widget instances (for example, on two panels) share it and never probe twice.
- **Transcripts:** `~/.claude/projects/**/*.jsonl`, watched with inotify and debounced. The live index stays in memory and is rebuilt at launch (ADR-024), with incremental tail reads as `IncrementalJSONLReader` does.
- **Storage (XDG):**

  | What | Where | Format |
  |---|---|---|
  | Last results, notification keys, activity cache | `$XDG_DATA_HOME/contribusage/` (`~/.local/share/contribusage/`) | versioned atomic JSON, as ADR-005: write a temporary file, then rename |
  | Caches safe to delete | `$XDG_CACHE_HOME/contribusage/` | JSON |
  | All settings: jobs, intervals, label style, look, theme tokens, layout, block order | plasmoid config (KConfigXT `main.xml`) | KConfig |
  | GitHub token | KWallet over the Secret Service API | — |

  Settings live only in the plasmoid config: Plasma's config dialog edits it, QML binds to it directly, and there is no second settings store.
- **Status line bridge** (ADR-002, optional): the same contract, with its file in the data folder above.

## 3. UI, look and feel, and customization

**Base look: a match of the macOS app.**

- **Panel label:** the six drawn styles from ADR-035, drawn with QtQuick Shapes, sharp at any panel size and colored the same way.
- **Popup:** 360 px wide, with the same blocks in the user's order: provider groups (limits meters, activity, collapsible insights; sections reorderable per ADR-034), the shared heatmap (ADR-032, Combined or Stacked), GitHub statistics, then the footer. The same loading, error and empty states.
- **Material:** a translucent background that KWin blurs, in place of macOS vibrancy.
- **Known differences:**
  - SF Pro cannot be shipped on Linux, so the default font is Inter, falling back to the system font.
  - SF Symbols are replaced by bundled SVGs drawn in the same style.
- **Heatmap:** a QML `Repeater` of rectangles, with no chart library.

**Customization: three layers, each falling back to the one below.**

1. **Look:** `macOS` (the default), `Plasma` (Kirigami theme colors and system fonts, so the widget blends with Breeze or any other theme), or `Custom`.
2. **Theme tokens:** one `Theme` QML singleton holds every color, font, size, spacing, radius and palette (meters, heatmap). `Custom` overrides any token from the config dialog. Density (compact or comfortable), popup width and label size are tokens as well.
3. **QML overrides:** each block is loaded through a `Loader`. If `$XDG_DATA_HOME/contribusage/qml/<Block>.qml` exists, it replaces the built-in block and receives the same documented, versioned properties (its view model and the theme). If the override fails to load or reports an error, the built-in block is used and a small notice says so. Overrides run as code inside plasmashell; the documentation says so.

**Settings:** Plasma's config dialog, with the tabs General, Providers, GitHub, Popup (block order by drag, visibility, heatmap) and Appearance (look, tokens, override folder).

## 4. Errors, testing, resource budget, order

**Error handling.**
- The same error cases as today: `claude` not found, unsupported plan (ADR-017), probe timeout, parse failure, GitHub 401 and rate limit (ADR-020, ADR-022), offline.
- The C++ code returns `std::expected` results and never throws across the QML boundary. Every error becomes a state the block can show.
- The probe keeps its 30 s timeout, then SIGTERM, then SIGKILL after 2 s, and one probe process at a time.
- A corrupt JSON file is moved aside and rebuilt from the next fetch.

**Testing.**
- The Swift tests are ported one-to-one to Qt Test, module by module, using the same fixture files (they are plain text). Fakes stand in for every seam, and the provider conformance suite carries over.
- A small `qmltestrunner` suite covers the override fallback and the theme layers.
- CI runs in an Arch Linux container on GitHub Actions: the build with `-Werror`, the tests, clang-format and `llm-wiki/tools/wiki.sh lint`.

**Resource budget.** These are checks, measured on the target machine:
- The plasmoid adds less than 15 MB to plasmashell's resident memory, measured as the RSS difference with and without the widget.
- Idle CPU is about 0 between refreshes. No timer runs faster than the next due job, and the minute tick runs only while the popup is open or the label shows a countdown.
- A refresh pass costs under 50 ms of CPU, not counting the `claude` process.

**Order.** Each milestone is usable and testable before the next one starts.

| Milestone | Scope |
|---|---|
| M0 | Reset the fork: CMake skeleton, CI, AGENTS.md and SPEC.md rewritten for Linux, ADR-041 for the port; remove the Swift and Xcode tree |
| M1 | `core/`, the seams and persistence, with the core tests ported |
| M2 | The Claude Code provider: probe, parser, transcripts, insights |
| M3 | GitHub |
| M4 | The plasmoid with the base look: label, popup, states |
| M5 | Settings pages and notifications |
| M6 | The customization layers |
| M7 | The PKGBUILD and the resource budget measurements |
