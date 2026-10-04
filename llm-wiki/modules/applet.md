---
type: module
status: active
updated: 2026-10-04
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
- QML singleton `AppInfo`: `version` (constant string).
- `main.qml`: a `PlasmoidItem` with placeholder compact and full representations until L-4.

## How it works
`plasma_add_applet(org.contribusage.widget … GENERATE_APPLET_CLASS)` builds one shared library holding the QML and the C++ types, with `metadata.json` embedded by moc. It is written to `build/bin/plasma/applets/`; Plasma finds it by id under `plasma/applets` in the Qt plugin paths. `tests/applet/tst_appletplugin.cpp` finds it the same way, and `tst_appletqml.cpp` loads the plugin, compiles `main.qml` and resolves the `AppInfo` singleton, because the build itself does not reject an unknown QML type.

## Things that bite
- Run it from the build tree with `QT_PLUGIN_PATH=$PWD/build/bin plasmoidviewer -a org.contribusage.widget`. An install to `~/.local` is not found by plasmashell unless its `QT_PLUGIN_PATH` includes it; the PKGBUILD (L-7) installs to `/usr`.
- libplasma 6.7's CMake package links `KF6::ConfigCore` without finding it, so the top-level `CMakeLists.txt` finds KF6 Config before Plasma.
- KDE's CMake settings register an `appstreamtest` CTest test of their own; test counts include it.

## Related
- [[decisions/0041-linux-plasmoid-port]]
