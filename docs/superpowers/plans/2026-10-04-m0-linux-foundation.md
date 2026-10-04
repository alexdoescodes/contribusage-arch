# M0: Linux Foundation Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Turn the fork into a buildable, tested, CI-checked KDE Plasma 6 widget skeleton: a static C++ logic library and a compiled plasmoid that loads in plasmashell. The macOS Swift tree goes, the contract documents are rewritten for Linux, and the vault stays lint-clean.

**Architecture:** One CMake project with KDE Extra CMake Modules. `src/core/` builds `contribusage_core`, a static, position-independent library on Qt6::Core that holds all logic in later milestones. `src/applet/` builds the plasmoid `org.contribusage.widget` with libplasma's `plasma_add_applet`, a shared library holding QML and C++ that links `contribusage_core`. Qt Test unit tests live in `tests/` and run through CTest.

**Tech Stack:** C++23, CMake ≥ 3.27, Ninja, extra-cmake-modules (KF6), Qt 6.8+ (Core, Gui, Qml, Quick, Test), libplasma 6, KF6 CoreAddons, clang-format, GitHub Actions on an `archlinux:latest` container.

**Spec:** `docs/superpowers/specs/2026-10-04-linux-plasmoid-port-design.md` (approved 2026-10-04). M0 is its first milestone: "Reset the fork: CMake skeleton, CI, AGENTS.md and SPEC.md rewritten for Linux, ADR-041 for the port; remove the Swift and Xcode tree."

## Global Constraints

- Linux only: Arch Linux, KDE Plasma 6 on Wayland. No macOS code is kept in the tree; the Swift implementation stays readable at the git tag `macos-reference`.
- Plasmoid id `org.contribusage.widget`; project version `0.1.0`.
- C++23, `-Werror`, clang-format (`.clang-format`: LLVM base, 4-space indent, 120 columns).
- `src/core/` links only Qt6::Core and never includes anything from `src/applet/`, or, in later milestones, a provider or GitHub directory (spec §1, "Boundaries as in the Swift code").
- Nothing blocks the plasmashell GUI thread (spec §1, "Async only"). M0 has no blocking work, but nothing added may break this rule.
- Rules carried over from SPEC §2: leave Claude Code's credentials untouched; call no vendor backend; run `claude` only with the arguments in SPEC §8.1.1 and FR-6; keep the GitHub token in the secret store and never in logs.
- The vault rules in AGENTS.md still hold: every change updates the affected pages, `index.md` and `log.md`, and `llm-wiki/tools/wiki.sh lint` must print `wiki lint: clean`.
- Commit messages follow the repo style (`Area: summary`) and end with `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.

## Review Focus

1. **A fresh clone needs the Swift fixtures in M2 after this milestone deletes them.** Expected: `git show macos-reference:<path>` works for every fixture. Pinned in Task 1, Step 3.
2. **Building on a machine without extra-cmake-modules or libplasma headers.** Expected: CMake stops at configure time and names the missing package, not a compiler error deep in the build. Pinned in Task 2, Step 6 (configure against a missing package) and by CI's clean container (Task 5).
3. **`-Werror` against moc-generated code and Qt headers on a newer compiler in CI than locally.** Expected: the CI build matches the local build. Pinned in Task 5: CI builds with the same flags, and the local gcc and the CI compiler versions are both printed.
4. **The plasmoid builds but Plasma cannot find or load it** (wrong id, metadata not embedded, wrong output directory). Expected: a test fails, not a silent empty panel. Pinned in Task 3, the `tst_appletplugin` test.
5. **Deleting the Swift tree leaves vault pages tracking missing paths, or the repo map listing removed folders.** Expected: `wiki.sh lint` is clean after Task 4. Pinned in Task 4, Steps 4 to 6.

---

## File Structure

| Path | Responsibility |
|---|---|
| `CMakeLists.txt` | Project, toolchain settings, packages, subdirectories |
| `.clang-format` | C++ format rules |
| `src/core/CMakeLists.txt` | `contribusage_core` static library |
| `src/core/version.h`, `src/core/version.cpp` | `contribusage::version()`: the first core symbol, proving the library and test wiring |
| `src/applet/CMakeLists.txt` | The `org.contribusage.widget` applet via `plasma_add_applet` |
| `src/applet/metadata.json` | Plasma plugin metadata |
| `src/applet/main.qml` | Compact representation (panel label) and full representation (popup), placeholders until M4 |
| `src/applet/appinfo.h`, `src/applet/appinfo.cpp` | `AppInfo` QML singleton exposing the core version to QML |
| `tests/CMakeLists.txt` | Test registration |
| `tests/core/tst_version.cpp` | Unit test for `contribusage::version()` |
| `tests/applet/tst_appletplugin.cpp` | Finds and reads the built applet plugin by id |
| `.github/workflows/ci.yml` | Linux CI: build, tests, format, wiki lint |
| `AGENTS.md`, `SPEC.md`, `README.md`, `CONTRIBUTING.md`, `SECURITY.md` | Contract and docs rewritten for Linux |
| `llm-wiki/…` | ADR-041, module pages, repo map, index, log |

Removed: `App/`, `Contribusage.xcodeproj/`, `Packages/`, `.swift-format`.

---

### Task 1: Prerequisites and the reference tag

**Files:** none in the tree. Creates the local git tag `macos-reference`.

**Interfaces:**
- Produces: the tag `macos-reference` at `2603a21` (the last macOS commit on `main`). M1 to M3 read the Swift sources and fixtures from it with `git show macos-reference:<path>`.

- [ ] **Step 1: Install the build tools (the user runs this; it needs sudo)**

```bash
sudo pacman -Syu --needed extra-cmake-modules ninja clang plasma-sdk
```

Expected: the packages are installed. `-Syu` is required because extra-cmake-modules in the repos (6.29) is newer than the installed KF6 (6.27), and Arch does not support partial upgrades.

- [ ] **Step 2: Check the toolchain**

Run:
```bash
cmake --version | head -1; ninja --version; g++ --version | head -1; clang-format --version
ls /usr/share/ECM/cmake/ECMConfig.cmake /usr/lib/cmake/Plasma/PlasmaMacros.cmake
```
Expected: cmake ≥ 3.27, ninja, gcc ≥ 14, clang-format, and both files exist.

- [ ] **Step 3: Tag the macOS reference and check that the fixtures are reachable**

```bash
git tag -a macos-reference 2603a21 -m "Last macOS (Swift) state; reference for the Linux port"
git show macos-reference:Packages/ContribusageKit/Tests/ContribusageClaudeCodeTests/Fixtures >/dev/null && echo fixtures-ok
git ls-tree -r --name-only macos-reference Packages/ContribusageKit/Tests | grep -c Fixtures/
```
Expected: `fixtures-ok`, then a count greater than 0. The tag stays local until the user decides to push (`git push origin macos-reference`).

---

### Task 2: CMake project and the core library, test first

**Files:**
- Create: `CMakeLists.txt`, `.clang-format`, `src/core/CMakeLists.txt`, `src/core/version.h`, `src/core/version.cpp`, `tests/CMakeLists.txt`, `tests/core/tst_version.cpp`
- Modify: `.gitignore` (add `build/`)

**Interfaces:**
- Produces: CMake target `contribusage_core` (STATIC, PIC, `PUBLIC` include root `src/`, so includes read `#include "core/version.h"`); `namespace contribusage { QString version(); }` returning `"0.1.0"`; test registration through `ecm_add_test` in `tests/CMakeLists.txt`.

- [ ] **Step 1: Write the failing test**

`tests/core/tst_version.cpp`:
```cpp
#include "core/version.h"

#include <QTest>

class TestVersion : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void reportsTheProjectVersion() { QCOMPARE(contribusage::version(), QStringLiteral("0.1.0")); }
};

QTEST_GUILESS_MAIN(TestVersion)
#include "tst_version.moc"
```

`tests/CMakeLists.txt`:
```cmake
include(ECMAddTests)

ecm_add_test(core/tst_version.cpp
    TEST_NAME tst_version
    LINK_LIBRARIES contribusage_core Qt6::Test)
```

`CMakeLists.txt`:
```cmake
cmake_minimum_required(VERSION 3.27)
project(contribusage VERSION 0.1.0 LANGUAGES CXX)

set(QT_MIN_VERSION 6.8)
set(KF_MIN_VERSION 6.10)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(ECM ${KF_MIN_VERSION} REQUIRED NO_MODULE)
set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH})

include(KDEInstallDirs)
include(KDECMakeSettings)
include(KDECompilerSettings NO_POLICY_SCOPE)
include(FeatureSummary)

find_package(Qt6 ${QT_MIN_VERSION} REQUIRED COMPONENTS Core Test)

# After the KDE settings, so their warning flags are in place before warnings become errors.
add_compile_options(-Werror)

add_subdirectory(src/core)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()

feature_summary(WHAT ALL FATAL_ON_MISSING_REQUIRED_PACKAGES)
```

`src/core/CMakeLists.txt` (empty library for now, so the test fails at link time):
```cmake
add_library(contribusage_core STATIC)
target_include_directories(contribusage_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_link_libraries(contribusage_core PUBLIC Qt6::Core)
# Linked into the shared applet library.
set_target_properties(contribusage_core PROPERTIES POSITION_INDEPENDENT_CODE ON)
```

`.gitignore`: append the line `build/`.

- [ ] **Step 2: Run the build and confirm the test fails**

Run: `cmake -B build -G Ninja && cmake --build build`
Expected: FAIL, with `core/version.h: No such file or directory` in `tst_version.cpp`.

- [ ] **Step 3: Write the minimal implementation**

`src/core/version.h`:
```cpp
#pragma once

#include <QString>

namespace contribusage {

// The project version from CMake, as shown in the popup footer and the diagnostics.
QString version();

} // namespace contribusage
```

`src/core/version.cpp`:
```cpp
#include "core/version.h"

namespace contribusage {

QString version() { return QStringLiteral(CONTRIBUSAGE_VERSION); }

} // namespace contribusage
```

`src/core/CMakeLists.txt`, replacing the first line and adding the definition:
```cmake
add_library(contribusage_core STATIC
    version.cpp
    version.h)
target_compile_definitions(contribusage_core PRIVATE CONTRIBUSAGE_VERSION="${PROJECT_VERSION}")
```
(Keep the `target_include_directories`, `target_link_libraries` and `set_target_properties` lines from Step 1.)

- [ ] **Step 4: Run the tests and confirm they pass**

Run: `cmake --build build && ctest --test-dir build --output-on-failure`
Expected: `100% tests passed, 0 tests failed out of 1`.

- [ ] **Step 5: Add `.clang-format` and check formatting**

`.clang-format`:
```yaml
BasedOnStyle: LLVM
IndentWidth: 4
ColumnLimit: 120
AccessModifierOffset: -4
AllowShortFunctionsOnASingleLine: Inline
```

Run: `clang-format --dry-run --Werror $(git ls-files -co --exclude-standard '*.cpp' '*.h')`
Expected: no output, exit 0. If it reports anything, run `clang-format -i` on those files and re-run.

- [ ] **Step 6: Check that a missing package fails at configure time (Review Focus 2)**

Run: `cmake -B /tmp/cu-missing -G Ninja -DCMAKE_DISABLE_FIND_PACKAGE_ECM=ON 2>&1 | grep -m1 ECM; rm -rf /tmp/cu-missing`
Expected: a CMake error line naming `ECM`, printed at configure time, before any compiler runs. This simulates a machine without extra-cmake-modules.

- [ ] **Step 7: Commit**

```bash
git add CMakeLists.txt .clang-format .gitignore src/core tests
git commit -m "Build: CMake project and the core library with its first test

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 3: The plasmoid skeleton, with a test that Plasma can find it

**Files:**
- Create: `src/applet/CMakeLists.txt`, `src/applet/metadata.json`, `src/applet/main.qml`, `src/applet/appinfo.h`, `src/applet/appinfo.cpp`, `tests/applet/tst_appletplugin.cpp`
- Modify: `CMakeLists.txt` (find Plasma, KF6 CoreAddons and Qt Gui, Qml and Quick; add `src/applet`), `tests/CMakeLists.txt` (add the applet test)

**Interfaces:**
- Consumes: `contribusage_core`, `contribusage::version()` (Task 2).
- Produces: the plugin `org.contribusage.widget.so` in `${CMAKE_BINARY_DIR}/bin/plasma/applets/`; QML singleton `AppInfo` with the `CONSTANT` property `version: string`, visible in the applet's own QML module (no import needed from `main.qml`).

- [ ] **Step 1: Write the failing test**

`tests/applet/tst_appletplugin.cpp`:
```cpp
#include <KPluginMetaData>
#include <QCoreApplication>
#include <QTest>

// Finds the built applet the way Plasma does: by id, under plasma/applets in the plugin paths.
class TestAppletPlugin : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void initTestCase() { QCoreApplication::addLibraryPath(QStringLiteral(CONTRIBUSAGE_PLUGIN_DIR)); }

    void appletIsFoundById() {
        const auto metaData =
            KPluginMetaData::findPluginById(QStringLiteral("plasma/applets"), QStringLiteral("org.contribusage.widget"));
        QVERIFY2(metaData.isValid(), "org.contribusage.widget not found under plasma/applets");
        QCOMPARE(metaData.name(), QStringLiteral("contribusage"));
        QCOMPARE(metaData.version(), QStringLiteral("0.1.0"));
        QCOMPARE(metaData.value(QStringLiteral("X-Plasma-API-Minimum-Version")), QStringLiteral("6.0"));
    }
};

QTEST_GUILESS_MAIN(TestAppletPlugin)
#include "tst_appletplugin.moc"
```

Append to `tests/CMakeLists.txt`:
```cmake
ecm_add_test(applet/tst_appletplugin.cpp
    TEST_NAME tst_appletplugin
    LINK_LIBRARIES KF6::CoreAddons Qt6::Test)
target_compile_definitions(tst_appletplugin PRIVATE CONTRIBUSAGE_PLUGIN_DIR="${CMAKE_BINARY_DIR}/bin")
```

In `CMakeLists.txt`, replace the `find_package(Qt6 …)` line with:
```cmake
find_package(Qt6 ${QT_MIN_VERSION} REQUIRED COMPONENTS Core Gui Qml Quick Test)
find_package(KF6 ${KF_MIN_VERSION} REQUIRED COMPONENTS CoreAddons)
find_package(Plasma REQUIRED)
```

- [ ] **Step 2: Run the tests and confirm the new one fails**

Run: `cmake -B build -G Ninja && cmake --build build && ctest --test-dir build --output-on-failure -R tst_appletplugin`
Expected: FAIL with `org.contribusage.widget not found under plasma/applets`.

- [ ] **Step 3: Write the applet**

`src/applet/metadata.json`:
```json
{
    "KPackageStructure": "Plasma/Applet",
    "KPlugin": {
        "Authors": [{ "Name": "contribusage contributors" }],
        "Category": "System Information",
        "Description": "Limits and activity of your AI coding tools, and your GitHub contributions",
        "Icon": "utilities-system-monitor",
        "Id": "org.contribusage.widget",
        "License": "MIT",
        "Name": "contribusage",
        "Version": "0.1.0"
    },
    "X-Plasma-API-Minimum-Version": "6.0"
}
```

`src/applet/appinfo.h`:
```cpp
#pragma once

#include <QObject>
#include <QtQml/qqmlregistration.h>

// Facts about the build for QML: the version in the popup footer.
class AppInfo : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
    Q_PROPERTY(QString version READ version CONSTANT)

public:
    using QObject::QObject;
    QString version() const;
};
```

`src/applet/appinfo.cpp`:
```cpp
#include "appinfo.h"

#include "core/version.h"

QString AppInfo::version() const { return contribusage::version(); }
```

`src/applet/main.qml`:
```qml
import QtQuick
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kde.plasma.components as PlasmaComponents
import org.kde.plasma.plasmoid

// Placeholder representations until M4 brings the drawn label and the popup blocks.
PlasmoidItem {
    id: root

    compactRepresentation: PlasmaComponents.Label {
        text: "contribusage"

        MouseArea {
            anchors.fill: parent
            onClicked: root.expanded = !root.expanded
        }
    }

    fullRepresentation: ColumnLayout {
        Layout.preferredWidth: Kirigami.Units.gridUnit * 20

        PlasmaComponents.Label {
            text: "contribusage " + AppInfo.version
        }
    }
}
```

`src/applet/CMakeLists.txt`:
```cmake
plasma_add_applet(org.contribusage.widget
    QML_SOURCES main.qml
    CPP_SOURCES appinfo.cpp appinfo.h
    GENERATE_APPLET_CLASS)
target_link_libraries(org.contribusage.widget PRIVATE contribusage_core Qt6::Qml Qt6::Quick)
```

In `CMakeLists.txt`, after `add_subdirectory(src/core)`:
```cmake
add_subdirectory(src/applet)
```

And in `tests/CMakeLists.txt`, after the `ecm_add_test(applet/…)` block, make the test build after the plugin:
```cmake
add_dependencies(tst_appletplugin org.contribusage.widget)
```

- [ ] **Step 4: Run all tests and confirm they pass**

Run: `cmake --build build && ctest --test-dir build --output-on-failure`
Expected: `100% tests passed, 0 tests failed out of 2`, and the file `build/bin/plasma/applets/org.contribusage.widget.so` exists.

If `tst_appletplugin` still fails while the `.so` exists, check the embedded metadata with `strings build/bin/plasma/applets/org.contribusage.widget.so | grep -m1 org.contribusage.widget`. Empty output means moc did not find `metadata.json`. In that case, add `target_include_directories(org.contribusage.widget PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})` to `src/applet/CMakeLists.txt` and rebuild.

- [ ] **Step 5: Look at it in Plasma's viewer (manual)**

Run: `QT_PLUGIN_PATH=$PWD/build/bin plasmoidviewer -a org.contribusage.widget -f horizontal`
Expected: a window with a panel showing the label `contribusage`; clicking it opens a popup reading `contribusage 0.1.0`. QML errors would appear in the terminal; there must be none.

- [ ] **Step 6: Format and commit**

Run: `clang-format --dry-run --Werror $(git ls-files -co --exclude-standard '*.cpp' '*.h')`. Expected: no output.

```bash
git add CMakeLists.txt src/applet tests
git commit -m "Applet: plasmoid skeleton org.contribusage.widget with the core version

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 4: Remove the macOS tree and keep the vault lint-clean

**Files:**
- Delete: `App/`, `Contribusage.xcodeproj/`, `Packages/`, `.swift-format`
- Modify: every vault page whose `tracks:` lists a deleted path (the list is in Step 3), `llm-wiki/modules/{app,core,claude-code,github,test-support}.md`, `llm-wiki/architecture/repo-map.md`
- Create: `llm-wiki/modules/applet.md`

**Interfaces:**
- Consumes: the tag `macos-reference` (Task 1) as the place the Swift code is read from now on.

- [ ] **Step 1: Delete the Swift and Xcode tree**

```bash
git rm -r -q App Contribusage.xcodeproj Packages .swift-format
```

- [ ] **Step 2: Run the lint and confirm it fails**

Run: `llm-wiki/tools/wiki.sh lint`
Expected: FAIL with lines such as `modules/core: tracks a missing path: Packages/ContribusageKit/Sources/ContribusageCore` and `architecture/repo-map: the tree lists App, which does not exist`.

- [ ] **Step 3: Drop deleted paths from every `tracks:` list**

These pages describe decisions and findings that still hold for the port; they lose only their links to Swift files. Later milestones re-link them when the C++ code lands.

```bash
python3 - <<'EOF'
import pathlib, re
gone = ("App", "Contribusage.xcodeproj", "Packages")
for page in pathlib.Path("llm-wiki").rglob("*.md"):
    if "sessions" in page.parts or "_templates" in page.parts:
        continue
    text = page.read_text()
    m = re.search(r"^tracks: \[(.*)\]$", text, re.M)
    if not m or not m.group(1).strip():
        continue
    paths = [p.strip() for p in m.group(1).split(",")]
    kept = [p for p in paths if not p.split("/")[0] in gone]
    if kept != paths:
        text = text.replace(m.group(0), f"tracks: [{', '.join(kept)}]")
        text = re.sub(r"^updated: .*$", "updated: 2026-10-04", text, count=1, flags=re.M)
        page.write_text(text)
        print(page)
EOF
```
Expected: about 35 page paths are printed (decisions, research and modules pages).

- [ ] **Step 4: Point the module pages at the port**

Edit `llm-wiki/modules/app.md`: set `status: deprecated` and put this line directly under the title:
```markdown
> The macOS SwiftUI app is replaced by [[modules/applet]] in the Linux port (ADR-041). The Swift code is at the git tag `macos-reference`.
```

Edit `llm-wiki/modules/core.md`, `modules/claude-code.md`, `modules/github.md` and `modules/test-support.md`: set `status: planned` and put this line directly under each title (with the milestone that page's code lands in: core and test-support M1, claude-code M2, github M3):
```markdown
> Being ported to C++ for the Linux port (ADR-041), milestone M<n>. Until then this page describes the Swift implementation at the git tag `macos-reference`.
```

Create `llm-wiki/modules/applet.md`:
```markdown
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
- libplasma 6 (`plasma_add_applet`), KF6 CoreAddons, Qt Quick

## Contract
- QML singleton `AppInfo`: `version` (constant string).
- `main.qml`: a `PlasmoidItem` with placeholder compact and full representations until L-4.

## How it works
`plasma_add_applet(org.contribusage.widget … GENERATE_APPLET_CLASS)` builds one shared library holding the QML and the C++ types, with `metadata.json` embedded by moc. It is written to `build/bin/plasma/applets/`; Plasma finds it by id under `plasma/applets` in the Qt plugin paths. `tests/applet/tst_appletplugin.cpp` finds it the same way.

## Things that bite
- Run it from the build tree with `QT_PLUGIN_PATH=$PWD/build/bin plasmoidviewer -a org.contribusage.widget`. An install to `~/.local` is not found by plasmashell unless its `QT_PLUGIN_PATH` includes it; the PKGBUILD (L-7) installs to `/usr`.

## Related
- [[decisions/0041-linux-plasmoid-port]]
```

- [ ] **Step 5: Rewrite the repo map tree**

In `llm-wiki/architecture/repo-map.md`, set `tracks: [.github, src, tests, CMakeLists.txt, docs, LICENSE]` and `updated: 2026-10-04`. In the first code block, remove the lines for `.swift-format`, `App/`, `Contribusage.xcodeproj` and `Packages/`, and add these lines in alphabetical position (the lint compares top-level names only):
```text
├── .clang-format           clang-format settings (LLVM base, 4 spaces, 120 columns)
├── CMakeLists.txt          the CMake project: KDE ECM settings, -Werror, src/ and tests/
├── src/                    core/: the static logic library contribusage_core (Qt6::Core only); applet/: the Plasma widget org.contribusage.widget (QML and its C++ view models)
├── tests/                  Qt Test suites run by CTest: core/, applet/
```
Change the paragraph under the title so it cites the port instead of SPEC §15.2:
```markdown
Every top-level entry of the repository and of the vault, and what it is for. `wiki.sh lint` compares the first code block below with the files on disk (the repository root and the vault's top level), so the tree cannot drift unnoticed. The source layout of the Linux port is in its design (`docs/superpowers/specs/2026-10-04-linux-plasmoid-port-design.md` §1); each build target has its page under `modules/`. The macOS layout of SPEC §15.2 is at the git tag `macos-reference`.
```

- [ ] **Step 6: Run the lint and confirm it is clean**

Run: `llm-wiki/tools/wiki.sh lint`
Expected: `wiki lint: clean`. Any remaining finding names a page and what to fix; fix it and re-run. Note that `modules/applet.md` links to `[[decisions/0041-linux-plasmoid-port]]`, which Task 6 creates. If the lint reports that link as broken, leave it and expect it to be clean after Task 6; this is the only finding allowed to remain.

- [ ] **Step 7: Confirm the build is unaffected and commit**

Run: `cmake --build build && ctest --test-dir build --output-on-failure`
Expected: `100% tests passed … out of 2`.

```bash
git add -A App Contribusage.xcodeproj Packages .swift-format llm-wiki
git commit -m "Port: remove the macOS Swift tree; vault pages unlink it

The Swift code stays readable at the tag macos-reference.

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 5: Linux CI

**Files:**
- Modify (replace whole file): `.github/workflows/ci.yml`

**Interfaces:**
- Consumes: the CMake project and tests (Tasks 2 and 3), `llm-wiki/tools/wiki.sh` and `test-wiki.sh`.

- [ ] **Step 1: Check that the wiki tools run on Linux**

Run: `llm-wiki/tools/test-wiki.sh`
Expected: it passes on this Linux machine. If it fails because of a macOS-only command (BSD `stat`, `date -j`, `sed -i ''`), stop and report; fixing `wiki.sh` is out of scope for this task.

- [ ] **Step 2: Replace the workflow**

`.github/workflows/ci.yml`:
```yaml
# Build, tests and format in an Arch Linux container, plus the wiki lint, on every push and pull request.
name: CI

on:
  push:
    branches: [main, linux-port]
  pull_request:

permissions:
  contents: read

concurrency:
  group: ${{ github.workflow }}-${{ github.ref }}
  cancel-in-progress: true

jobs:
  build:
    name: Build, tests and format
    runs-on: ubuntu-latest
    container: archlinux:latest
    steps:
      - name: Toolchain
        run: |
          pacman -Syu --noconfirm --needed base-devel git cmake ninja clang extra-cmake-modules \
            qt6-base qt6-declarative libplasma kcoreaddons kirigami
          g++ --version | head -1
      - uses: actions/checkout@v5
      - run: git config --global --add safe.directory "$GITHUB_WORKSPACE"
      - run: cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
      - run: cmake --build build
      - run: ctest --test-dir build --output-on-failure
        env:
          QT_QPA_PLATFORM: offscreen
      - run: clang-format --dry-run --Werror $(git ls-files '*.cpp' '*.h')

  wiki:
    name: Wiki lint
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v5
        with:
          fetch-depth: 0 # the stale-page check compares commit times
      - run: llm-wiki/tools/wiki.sh lint
      - run: llm-wiki/tools/test-wiki.sh
```

- [ ] **Step 3: Run the CI steps locally**

Run:
```bash
g++ --version | head -1
rm -rf build && cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release && cmake --build build \
  && QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure \
  && clang-format --dry-run --Werror $(git ls-files '*.cpp' '*.h') && echo ci-local-ok
```
Expected: `ci-local-ok`. Note the local gcc version printed; when CI runs (after the user pushes), compare it with the version CI prints (Review Focus 3).

- [ ] **Step 4: Commit**

```bash
git add .github/workflows/ci.yml
git commit -m "CI: Linux build, tests and format in an Arch container; wiki lint

Replaces the macOS jobs and the DMG release, which return as a package in L-7.

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 6: The contract and the docs for Linux, and ADR-041

**Files:**
- Create: `llm-wiki/decisions/0041-linux-plasmoid-port.md`
- Modify: `AGENTS.md`, `SPEC.md`, `README.md`, `CONTRIBUTING.md`, `SECURITY.md`, `llm-wiki/overview.md`, `llm-wiki/index.md`, `llm-wiki/log.md`

**Interfaces:**
- Produces: SPEC §17.8 with tasks L-0 to L-7, which later milestone plans tick off; ADR-041.

- [ ] **Step 1: Write ADR-041**

`llm-wiki/decisions/0041-linux-plasmoid-port.md`:
```markdown
---
type: decision
status: accepted
updated: 2026-10-04
aliases: [ADR-041]
tracks: [CMakeLists.txt, src]
tags: [platform, architecture]
---
# ADR-041: Linux-only port as a KDE Plasma widget

- **Decided:** 2026-10-04 · **Spec:** SPEC header, §1.3, §2, §17.8 · **Supersedes:** [[decisions/0001-native-swift-swiftui]], [[decisions/0008-minimum-macos-14]], [[decisions/0009-apple-silicon-only]] for this fork

## Context
This fork targets Arch Linux on KDE Plasma 6 (Wayland) and drops macOS. The goal is the least use of system resources, the macOS look by default, and a customizable appearance. The full design is in `docs/superpowers/specs/2026-10-04-linux-plasmoid-port-design.md`.

## Options
| Option | For | Against |
|---|---|---|
| C++/Qt logic library inside a compiled plasmoid | No extra process, no IPC, no second runtime; everything in the Arch repos | About 3,000 lines of logic rewritten; a crash in our code takes down plasmashell |
| Rust daemon and a QML plasmoid over D-Bus | Crash isolation, small memory | A second process, a D-Bus API to maintain, two languages |
| Swift daemon reusing `ContribusageKit` | Logic and tests kept | About 1 GB toolchain, about 30 MB runtime, Foundation gaps on Linux, no mature D-Bus library |

## Decision
A compiled Plasma 6 applet, `org.contribusage.widget`, built with `plasma_add_applet`, links a static C++23 library `contribusage_core` that holds all logic. The Swift tests and fixtures are ported one-to-one to Qt Test; the Swift code stays at the git tag `macos-reference`.

## Consequences
- macOS is no longer built or maintained here. ADR-001, ADR-008 and ADR-009 no longer apply to this fork; the other ADRs keep their design and are re-linked to C++ files as each milestone lands.
- The probe's own cost (R-1) dominates the resource budget, so its interval rules (ADR-039) stay.
- Revisit if plasmashell stability suffers from the in-process logic: the Rust daemon option is the fallback.
```

- [ ] **Step 2: Rewrite the platform parts of AGENTS.md**

Replace the paragraph starting `**contribusage** is a native macOS menu bar app` with:
```markdown
**contribusage** is a KDE Plasma 6 widget for Arch Linux (C++23, Qt 6, QML) that shows the usage of AI coding tools (v1: Claude Code, behind a provider framework) and GitHub contributions. This fork is the Linux port of the macOS app (ADR-041); the Swift code it was ported from is at the git tag `macos-reference`. Two documents govern the work, and every piece of knowledge lives in exactly one of them:
```

In "2. Work", replace step 2 and step 4 with:
```markdown
2. Write the tests first, as Qt Test suites under `tests/`, porting the matching Swift test and its fixtures from `macos-reference` where one exists.
```
```markdown
4. Run the tests (see Commands); look at the widget in `plasmoidviewer` too when the applet changed.
```

Replace the whole `## Hard rules (SPEC §2)` section with:
```markdown
## Hard rules (SPEC §2)

- Leave every tool's credentials untouched (for Claude Code: its credential file and any Secret Service or KWallet items it owns), and call no vendor backend (`api.anthropic.com`, `claude.ai`, or any other tool vendor's) from the widget's code.
- Run `claude` only with the arguments in SPEC §8.1.1 and FR-6.
- The GitHub token goes only through `SecretStore` (KWallet over the Secret Service API) and never into logs.
- `src/core/` links only Qt6::Core and includes no provider directory, no GitHub directory and nothing from `src/applet/`. Provider code lives only in its provider directory, and only after its provider section exists in SPEC §8.
- The applet holds QML views, their wiring and C++ view models only; nothing blocks the plasmashell GUI thread.
- Linux only: Arch Linux, KDE Plasma 6. No macOS code in the tree.
- C++23, `-Werror`, clang-format clean.
- A failing test is fixed in the code. A wrong fixture is corrected with the reason in the commit message.
```

Replace the whole `## Commands` table with:
```markdown
## Commands

| Purpose | Command |
|---|---|
| Configure | `cmake -B build -G Ninja` |
| Build | `cmake --build build` |
| Tests | `QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure` |
| One test | `ctest --test-dir build --output-on-failure -R tst_version` |
| Format check | `clang-format --dry-run --Werror $(git ls-files '*.cpp' '*.h')` |
| Run the widget | `QT_PLUGIN_PATH=$PWD/build/bin plasmoidviewer -a org.contribusage.widget -f horizontal` |
| CI | `.github/workflows/ci.yml` builds, tests and format-checks in an Arch Linux container, and runs the wiki lint, on every push and pull request |
| Wiki | `llm-wiki/tools/wiki.sh context`, `new-session <slug> [agent]`, `lint` |
| Wiki tooling test | `llm-wiki/tools/test-wiki.sh` (after changing `wiki.sh`) |
```
(Keep the `## Agent skills` section that follows it unchanged.)

- [ ] **Step 3: Update SPEC.md for Linux**

In the header table, replace the `Platform` and `Language` rows and the tagline:
```markdown
> A KDE Plasma 6 widget for Arch Linux that shows the usage of your AI coding tools (starting with Claude Code) and your GitHub contributions at a glance. Ported from the macOS app (ADR-041).
```
```markdown
| **Platform** | Arch Linux, KDE Plasma 6 (Wayland) |
| **Language** | C++23, Qt 6, QML |
```

In §1.3, replace the bullets `**No Intel Macs.** …` and `No Windows or Linux version.` and `No Mac App Store distribution …` with:
```markdown
- **No macOS or Windows version in this fork.** The macOS app is at the git tag `macos-reference` ([ADR-041](llm-wiki/decisions/0041-linux-plasmoid-port.md)).
- No desktop environments other than KDE Plasma 6.
```

In §2, replace principle 7 with:
```markdown
7. **Testable core.** All parsing and logic lives in the `contribusage_core` library (`src/core/` and its provider and GitHub directories), is covered by fixture based Qt Test suites, and is written test first.
```

At the top of §15 (directly under `## 15. Project setup`) and of §20 (directly under its heading), insert:
```markdown
> **macOS reference.** This section describes the macOS app at the git tag `macos-reference`. The Linux build is described in the port design (`docs/superpowers/specs/2026-10-04-linux-plasmoid-port-design.md`) and in AGENTS.md § Commands.
```

Directly before `### 17.7 Definition of Done`, insert:
```markdown
### 17.8 Linux port (ADR-041)

The milestones of the port design (`docs/superpowers/specs/2026-10-04-linux-plasmoid-port-design.md` §4). Each has its own plan in `docs/superpowers/plans/`.

- [x] **L-0** Foundation: CMake project, `contribusage_core`, the plasmoid skeleton `org.contribusage.widget`, Linux CI, the macOS tree removed, this contract updated. Accept: the CI steps pass locally (CI on GitHub after the branch is pushed); `tst_version` and `tst_appletplugin` pass; the widget shows its version in `plasmoidviewer`.
- [ ] **L-1** Core, seams and persistence in C++, with the core tests ported from `macos-reference`.
- [ ] **L-2** Claude Code provider: probe, parser, transcripts, insights, with its tests and fixtures ported.
- [ ] **L-3** GitHub: client, token validation, statistics, with its tests ported.
- [ ] **L-4** Widget with the base look: drawn label, popup blocks, states.
- [ ] **L-5** Settings pages and notifications.
- [ ] **L-6** Customization: look, theme tokens, QML overrides with fallback.
- [ ] **L-7** PKGBUILD and the resource budget measurements.
```
(L-0 is ticked in this same task because this task completes M0.)

In §17.7, replace the bullets `Zero warnings in Swift 6 language mode.`, `Every new UI state has a preview; VoiceOver labels present.` and `No main actor blocking; …` with:
```markdown
- Zero warnings with `-Werror`; clang-format clean.
- Every new UI state is checked in `plasmoidviewer`; accessible names set on interactive items.
- Nothing blocks the plasmashell GUI thread; no new timers while the popup is closed unless the spec allows it.
```

- [ ] **Step 4: Update README.md, CONTRIBUTING.md and SECURITY.md**

README.md:
- Replace the three badge lines for macOS, Apple Silicon and Swift with:
```html
  <img src="https://img.shields.io/badge/KDE%20Plasma-6-1d99f3?style=plastic&logo=kde&logoColor=white" alt="KDE Plasma 6">
  <img src="https://img.shields.io/badge/Arch%20Linux-1793d1?style=plastic&logo=archlinux&logoColor=white" alt="Arch Linux">
  <img src="https://img.shields.io/badge/C%2B%2B-23-00599c?style=plastic&logo=cplusplus&logoColor=white" alt="C++23">
```
- In the CI badge, replace both occurrences of `kuehn-lars/contribusage` with `alexdoescodes/contribusage-arch`.
- Replace everything from `## Requirements` up to (not including) `## Roadmap` with:
```markdown
## Requirements

- Arch Linux with KDE Plasma 6
- Claude Code installed and logged in, for its limits and activity
- A GitHub token, for contributions (optional)

## Status

This fork is the Linux port of the macOS app, in progress (SPEC §17.8). The widget builds and loads; its features arrive milestone by milestone. A package comes with L-7.

## Build from source

```bash
sudo pacman -Syu --needed base-devel cmake ninja extra-cmake-modules qt6-base qt6-declarative libplasma kcoreaddons kirigami plasma-sdk
cmake -B build -G Ninja
cmake --build build
QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure
QT_PLUGIN_PATH=$PWD/build/bin plasmoidviewer -a org.contribusage.widget -f horizontal
```

```

CONTRIBUTING.md: replace the body of `## Build and run` (everything up to `## Find your way around`) with:
```markdown
You need Arch Linux with KDE Plasma 6. The commands are in [AGENTS.md § Commands](AGENTS.md#commands) and in the README's "Build from source". Run the tests and the format check before opening a pull request.

```

SECURITY.md: replace the body of `## Verify a download` (everything up to `## Not a vulnerability`) with:
```markdown
There are no Linux releases yet. Until the package arrives (SPEC §17.8, L-7), build from source.

```

Then search the three files for leftover macOS terms and fix any sentence that is now wrong:
Run: `grep -n -i -E "macos|xcode|swift|dmg|apple|mac " README.md CONTRIBUTING.md SECURITY.md`
Expected after fixing: matches only in sentences that name the macOS app as the origin of the port.

- [ ] **Step 5: Update the vault hub, index and log**

`llm-wiki/overview.md`: set `updated: 2026-10-04`, and add this paragraph directly under the title:
```markdown
> **Linux port.** This fork is being ported to a KDE Plasma 6 widget ([[decisions/0041-linux-plasmoid-port]]). The targets are `contribusage_core` ([[modules/core]]) and the applet ([[modules/applet]]); the macOS app ([[modules/app]]) is at the git tag `macos-reference`.
```

`llm-wiki/index.md`: under `## Modules`, add after the `modules/app` line:
```markdown
- [[modules/applet]]: the KDE Plasma 6 widget; QML views and C++ view models
```
Under `## Decisions`, add after the ADR-040 line:
```markdown
- [[decisions/0041-linux-plasmoid-port]]: ADR-041, Linux-only port as a compiled Plasma widget with a static C++ logic library
```

`llm-wiki/log.md`: append:
```markdown

## [2026-10-04] chore | Linux foundation (L-0)
- CMake project with `contribusage_core` and the plasmoid `org.contribusage.widget`; Qt Test suites under `tests/`; CI in an Arch container.
- The macOS tree is removed (tag `macos-reference`); AGENTS.md, SPEC.md and the README describe the Linux port; ADR-041 records the decision.
```

- [ ] **Step 6: Run every check**

Run:
```bash
llm-wiki/tools/wiki.sh lint
cmake --build build && QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure
clang-format --dry-run --Werror $(git ls-files '*.cpp' '*.h') && echo format-ok
```
Expected: `wiki lint: clean`, `100% tests passed … out of 2`, `format-ok`.

- [ ] **Step 7: Commit**

```bash
git add AGENTS.md SPEC.md README.md CONTRIBUTING.md SECURITY.md llm-wiki
git commit -m "Spec: the contract and the docs describe the Linux port; ADR-041

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```
