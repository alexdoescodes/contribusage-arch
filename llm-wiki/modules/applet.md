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
- QML singletons `AppInfo` (`version`), `PopupModel` (`providers`, `heatmap`, `github`, `label`; real Claude Code limits and insights since L-2a; the macOS preview data under CONTRIBUSAGE_FAKE_DATA=1) and `Theme` (every color, size and opacity; the customization layers of L-6 override it).
- `main.qml`: a `PlasmoidItem` with `CompactLabel` (the `prompt` style) and `FullPopup` (the blocks of SPEC §11.2: `ProviderGroup` with `LimitBar`, `ActivitySection` and `InsightsSection`, then `HeatmapBlock`, `GitHubSection` and `Footer`).

## How it works
`plasma_add_applet(org.contribusage.widget … GENERATE_APPLET_CLASS)` builds one shared library holding the QML and the C++ types, with `metadata.json` embedded by moc. It is written to `build/bin/plasma/applets/`; Plasma finds it by id under `plasma/applets` in the Qt plugin paths. `tests/applet/tst_appletplugin.cpp` finds it the same way, and `tst_appletqml.cpp` loads the plugin, compiles `main.qml` and resolves the `AppInfo` singleton, because the build itself does not reject an unknown QML type.

## Things that bite
- Run it from the build tree with `QT_PLUGIN_PATH=$PWD/build/bin plasmoidviewer -a org.contribusage.widget`. An install to `~/.local` is not found by plasmashell unless its `QT_PLUGIN_PATH` includes it; the PKGBUILD (L-7) installs to `/usr`.
- libplasma 6.7's CMake package links `KF6::ConfigCore` without finding it, so the top-level `CMakeLists.txt` finds KF6 Config before Plasma.
- KDE's CMake settings register an `appstreamtest` CTest test of their own; test counts include it.
- Colors live in `Theme.qml` only; text colors come from `Kirigami.Theme` in each component, so the popup follows a light or dark Plasma theme.
- `switchWidth`/`switchHeight` make a desktop (and `plasmoidviewer -f planar`) show the popup inline, which is how its look is checked.
- The panel label shows only inside a panel containment: `plasmoidviewer -c org.kde.panel -f horizontal -l topedge -s 500x220` (or `-f vertical -l leftedge -s 220x500`). With `-f` alone the viewer's desktop shows the popup inline.

- In live mode the activity, heatmap and GitHub blocks are hidden until their data exists (L-2b, L-3); `CONTRIBUSAGE_FAKE_DATA=1 plasmoidviewer …` shows the full preview.
## Related
- [[decisions/0041-linux-plasmoid-port]]
