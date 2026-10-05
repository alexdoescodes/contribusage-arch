---
type: module
status: active
updated: 2026-10-05
tracks: [src/applet, tests/applet]
tags: [plasma, qml]
---
# Applet

The KDE Plasma 6 widget `org.contribusage.widget`: QML views and their wiring, with C++ view models; no logic, which lives in `contribusage_core` ([[modules/core]]).

## Spec
- **Sections:** the port design `docs/superpowers/specs/2026-10-04-linux-plasmoid-port-design.md` §1 and §3
- **Tasks:** SPEC §17.8, L-0 (skeleton) and L-4 to L-6 (UI)
- **Path:** `src/applet/`

## Depends on
- [[modules/core]] through the static library `contribusage_core`
- libplasma 6 (`plasma_add_applet`), KF6 CoreAddons and Config, Qt Quick

## Contract
- QML singletons `AppInfo` (`version`), `PopupModel` (`providers`, `heatmap`, `github`, `label`; real Claude Code limits and insights since L-2a, activity (`providers[0].activity`) and a heatmap stacking the Claude Code and GitHub layers since L-2b; the macOS preview data under CONTRIBUSAGE_FAKE_DATA=1) and `Theme` (every color, size and opacity; the customization layers of L-6 override it).
- `main.qml`: a `PlasmoidItem` with `CompactLabel` (the `prompt` style) and `FullPopup` (the blocks of SPEC §11.2: `ProviderGroup` with `LimitBar`, `ActivitySection` and `InsightsSection`, then `HeatmapBlock`, `GitHubSection` and `Footer`).
- Since L-4: `LabelContent.qml` draws the label in the seven FR-51 styles (`RingGlyph`, `LineGlyph`, `MiniHeatmap`, `PromptGlyph`) from plain values; `CompactLabel` feeds it from `Plasmoid.configuration` (`main.xml` at the module root, where Plasma reads it as for KDE's compiled applets; source file `src/applet/config/main.xml`). `Skeleton.qml` and `ActionButton.qml` (Retry, Locate…, Show raw output) render SPEC §11.3; the heatmap walks with the arrow keys, Home and End (SPEC §11.7). A size bound to an item's own height inside a layout can loop without end: `everyStyleStaysSmallInAWindow` lays each style out in a window.
- Since L-5a: settings pages `ConfigGeneral`, `ConfigClaudeCode`, `ConfigGitHub`, `ConfigAdvanced` listed by `config.qml` (also at the module root); `main.qml` passes `Plasmoid.configuration` to `PopupModel.applySettings` at start and on change; `deleteClaudeCodeData()`, `resetCaches()`, `dataFolder`; the limits job's notes go to `DesktopNotifier` ([[decisions/0046-notifications-and-settings]]).
- `PopupModel.refresh()` (the Footer's Refresh) and `popupOpened()` (from `main.qml`); the jobs' schedules share a `ScheduleConditionsSource` fed by `SystemConditions` (logind, QNetworkInformation) since L-1c.
- `PopupModel.connectGitHub(token)` and `disconnectGitHub()`; `github` and `heatmap` are live (GitHub layer) since L-3.

## How it works
`plasma_add_applet(org.contribusage.widget … GENERATE_APPLET_CLASS)` builds one shared library holding the QML and the C++ types, with `metadata.json` embedded by moc. It is written to `build/bin/plasma/applets/`; Plasma finds it by id under `plasma/applets` in the Qt plugin paths. `tests/applet/tst_appletplugin.cpp` finds it the same way, and `tst_appletqml.cpp` loads the plugin, compiles `main.qml` and resolves the `AppInfo` singleton, because the build itself does not reject an unknown QML type.

## Things that bite
- Run it from the build tree with `QT_PLUGIN_PATH=$PWD/build/bin plasmoidviewer -a org.contribusage.widget`. An install to `~/.local` is not found by plasmashell unless its `QT_PLUGIN_PATH` includes it; the PKGBUILD (L-7) installs to `/usr`.
- libplasma 6.7's CMake package links `KF6::ConfigCore` without finding it, so the top-level `CMakeLists.txt` finds KF6 Config before Plasma.
- KDE's CMake settings register an `appstreamtest` CTest test of their own; test counts include it.
- Colors live in `Theme.qml` only; text colors come from `Kirigami.Theme` in each component, so the popup follows a light or dark Plasma theme.
- `switchWidth`/`switchHeight` make a desktop (and `plasmoidviewer -f planar`) show the popup inline, which is how its look is checked.
- The panel label shows only inside a panel containment: `plasmoidviewer -c org.kde.panel -f horizontal -l topedge -s 500x220` (or `-f vertical -l leftedge -s 220x500`). With `-f` alone the viewer's desktop shows the popup inline.

- In live mode the activity block is hidden until its data exists (L-2b); the GitHub block shows an inline token form until a token is connected, and the heatmap shows GitHub's layer once contributions are fetched; `CONTRIBUSAGE_FAKE_DATA=1 plasmoidviewer …` shows the full preview.
- The Insights open state and period live in `FullPopup.insightsState`, not in the section: live mode rebuilds the provider rows every minute.

## Related
- [[decisions/0041-linux-plasmoid-port]]
