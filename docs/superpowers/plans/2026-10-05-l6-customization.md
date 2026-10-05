# L-6: Customization — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** The port design's three customization layers: a look (`macOS`, `Plasma`, `Custom`), theme tokens a user can override (colors, sizes, opacities, density, popup width), and QML overrides for the popup's blocks with a safe fallback.

**Architecture:**
- **Core:** `appearance.h` validates the appearance settings: the look, the density and a JSON object of token overrides, keeping only known tokens of the right kind (`themeTokensFromText`).
- **Applet:** `PopupModel.appearance` (look, density, tokens) from `applySettings`; `Theme.qml` computes every token from the look, the density and the overrides (Plasma look: the system palette); `PopupModel.overrideUrl(block)` finds `$XDG_DATA_HOME/contribusage/qml/<Block>.qml`; `FullPopup` loads an override in place of the built-in block and falls back with a notice when it fails; an Appearance settings page.
- **Docs:** a guide page for overrides (properties per block, that overrides run inside plasmashell).
- **Rulings taken in the plan:** no bundled font (the system font, as the Plasma look does; Inter is not packaged here), no bundled SVG symbols (Breeze icon names already used); overrides cover the three popup blocks (`ProviderGroup`, `HeatmapBlock`, `GitHubSection`).

**Tech Stack:** C++23, Qt 6, QML, Qt Test.

**Spec:** port design `docs/superpowers/specs/2026-10-04-linux-plasmoid-port-design.md` §3 (customization: look, tokens, QML overrides); SPEC §11.4 (colors), §17.8 L-6.

## Global Constraints

- C++23, `-Werror`, clang-format clean; core links only Qt6::Core; `QStringLiteral`.
- The default look is unchanged (`macOS`): with no appearance settings every token keeps its current value.
- Override files are only read from the widget's own data folder; a failing override never leaves the block empty.

## Review Focus

1. **A token override with a wrong type** (`"padding": "big"`, `"accent": 12`). Expected: ignored, default kept. Pinned in Task 1 (`unknownAndWrongTokensAreDropped`).
2. **A broken override file** (syntax error). Expected: built-in block plus a notice. Pinned in Task 3 (`aBrokenOverrideFallsBack`).
3. **An override that loads.** Expected: it replaces the block and gets the block's data. Pinned in Task 3 (`anOverrideReplacesTheBlock`).
4. **Plasma look.** Expected: accent follows the system palette's highlight. Pinned in Task 2 (`plasmaLookFollowsThePalette`).
5. **Compact density.** Expected: smaller padding and spacing. Pinned in Task 2 (`densityScalesSpacing`).

---

### Task 1: Appearance settings in the core

**Files:** Create `src/core/appearance.h`, `src/core/appearance.cpp`, `tests/core/tst_appearance.cpp`; modify `src/core/CMakeLists.txt`, `tests/CMakeLists.txt`.

**Interfaces:** Produces (namespace `contribusage`): `QVariantMap themeTokensFromText(const QString &json)` (known color tokens `accent warning critical claudeHue githubHue` as `#rrggbb`/`#aarrggbb` strings; known number tokens `secondaryOpacity trackOpacity emptyCellOpacity dividerOpacity mutedBarOpacity` in 0..1, `popupWidth` 280..640, `padding spacing meterHeight cellGap weekChartHeight` 0..64; anything else dropped), `QVariantMap appearanceFromMap(const QVariantMap &configuration)` (keys `look` ∈ {`macOS`,`Plasma`,`Custom`} default `macOS`, `density` ∈ {`comfortable`,`compact`} default `comfortable`, `tokens` = `themeTokensFromText(themeTokens)` only for `Custom`, else empty).

- [ ] **Step 1: Write the failing test**

`tests/core/tst_appearance.cpp`:
```cpp
#include "core/appearance.h"

#include <QTest>

using namespace contribusage;

// Port design §3: the look, the density and the token overrides, validated.
class TestAppearance : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void defaultsAreTheMacOSLook() {
        const QVariantMap appearance = appearanceFromMap({});
        QCOMPARE(appearance.value(QStringLiteral("look")).toString(), QStringLiteral("macOS"));
        QCOMPARE(appearance.value(QStringLiteral("density")).toString(), QStringLiteral("comfortable"));
        QVERIFY(appearance.value(QStringLiteral("tokens")).toMap().isEmpty());
    }

    // Review Focus 1.
    void unknownAndWrongTokensAreDropped() {
        const QVariantMap tokens = themeTokensFromText(QStringLiteral(
            R"({"accent":"#112233","claudeHue":"#80ff0000","padding":20,"popupWidth":5000,"spacing":"big","warning":12,"secondaryOpacity":0.5,"nonsense":1,"critical":"red"})"));
        QCOMPARE(tokens.value(QStringLiteral("accent")).toString(), QStringLiteral("#112233"));
        QCOMPARE(tokens.value(QStringLiteral("claudeHue")).toString(), QStringLiteral("#80ff0000"));
        QCOMPARE(tokens.value(QStringLiteral("padding")).toInt(), 20);
        QCOMPARE(tokens.value(QStringLiteral("popupWidth")).toInt(), 640);
        QCOMPARE(tokens.value(QStringLiteral("secondaryOpacity")).toDouble(), 0.5);
        QVERIFY(!tokens.contains(QStringLiteral("spacing")));
        QVERIFY(!tokens.contains(QStringLiteral("warning")));
        QVERIFY(!tokens.contains(QStringLiteral("nonsense")));
        QVERIFY(!tokens.contains(QStringLiteral("critical")));
        QVERIFY(themeTokensFromText(QStringLiteral("{broken")).isEmpty());
    }

    void tokensApplyOnlyToTheCustomLook() {
        const QString tokens = QStringLiteral(R"({"accent":"#112233"})");
        QVERIFY(appearanceFromMap({{QStringLiteral("look"), QStringLiteral("Plasma")}, {QStringLiteral("themeTokens"), tokens}})
                    .value(QStringLiteral("tokens"))
                    .toMap()
                    .isEmpty());
        QCOMPARE(appearanceFromMap({{QStringLiteral("look"), QStringLiteral("Custom")}, {QStringLiteral("themeTokens"), tokens}})
                     .value(QStringLiteral("tokens"))
                     .toMap()
                     .value(QStringLiteral("accent"))
                     .toString(),
                 QStringLiteral("#112233"));
        QCOMPARE(appearanceFromMap({{QStringLiteral("look"), QStringLiteral("odd")}}).value(QStringLiteral("look")).toString(),
                 QStringLiteral("macOS"));
        QCOMPARE(appearanceFromMap({{QStringLiteral("density"), QStringLiteral("compact")}})
                     .value(QStringLiteral("density"))
                     .toString(),
                 QStringLiteral("compact"));
    }
};

QTEST_GUILESS_MAIN(TestAppearance)
#include "tst_appearance.moc"
```

Append to `tests/CMakeLists.txt`:
```cmake

ecm_add_test(core/tst_appearance.cpp
    TEST_NAME tst_appearance
    LINK_LIBRARIES contribusage_core Qt6::Test)
```

- [ ] **Step 2: Run it and confirm it fails** — `cmake -B build -G Ninja && cmake --build build` → FAIL with `core/appearance.h: No such file or directory`.

- [ ] **Step 3: Write it**

`src/core/appearance.h`:
```cpp
#pragma once

#include <QString>
#include <QVariantMap>

namespace contribusage {

// Port design §3, layer 2: token overrides from the `themeTokens` setting (a JSON object). Only known tokens of the
// right kind survive: colors as #rrggbb or #aarrggbb, opacities 0 to 1, sizes clamped to sane ranges.
QVariantMap themeTokensFromText(const QString &json);

// Port design §3: `look` (macOS, Plasma, Custom; default macOS), `density` (comfortable, compact) and `tokens` (the
// overrides, used by the Custom look only) from the plasmoid configuration.
QVariantMap appearanceFromMap(const QVariantMap &configuration);

} // namespace contribusage
```

`src/core/appearance.cpp`:
```cpp
#include "core/appearance.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>

#include <algorithm>

namespace contribusage {

namespace {

struct Range {
    double low;
    double high;
};

} // namespace

QVariantMap themeTokensFromText(const QString &json) {
    static const QRegularExpression color(QStringLiteral("^#([0-9a-fA-F]{6}|[0-9a-fA-F]{8})$"));
    static const QStringList colors{QStringLiteral("accent"), QStringLiteral("warning"), QStringLiteral("critical"),
                                    QStringLiteral("claudeHue"), QStringLiteral("githubHue")};
    static const QMap<QString, Range> numbers{
        {QStringLiteral("secondaryOpacity"), {0, 1}}, {QStringLiteral("trackOpacity"), {0, 1}},
        {QStringLiteral("emptyCellOpacity"), {0, 1}}, {QStringLiteral("dividerOpacity"), {0, 1}},
        {QStringLiteral("mutedBarOpacity"), {0, 1}},  {QStringLiteral("popupWidth"), {280, 640}},
        {QStringLiteral("padding"), {0, 64}},         {QStringLiteral("spacing"), {0, 64}},
        {QStringLiteral("meterHeight"), {0, 64}},     {QStringLiteral("cellGap"), {0, 64}},
        {QStringLiteral("weekChartHeight"), {0, 64}}};
    const QJsonDocument document = QJsonDocument::fromJson(json.toUtf8());
    QVariantMap tokens;
    if (!document.isObject()) {
        return tokens;
    }
    const QJsonObject object = document.object();
    for (auto it = object.begin(); it != object.end(); ++it) {
        if (colors.contains(it.key())) {
            if (it.value().isString() && color.match(it.value().toString()).hasMatch()) {
                tokens.insert(it.key(), it.value().toString());
            }
        } else if (const auto range = numbers.constFind(it.key()); range != numbers.cend() && it.value().isDouble()) {
            tokens.insert(it.key(), std::clamp(it.value().toDouble(), range->low, range->high));
        }
    }
    return tokens;
}

QVariantMap appearanceFromMap(const QVariantMap &configuration) {
    QString look = configuration.value(QStringLiteral("look")).toString();
    if (look != QStringLiteral("Plasma") && look != QStringLiteral("Custom")) {
        look = QStringLiteral("macOS");
    }
    const QString density = configuration.value(QStringLiteral("density")).toString() == QStringLiteral("compact")
                                ? QStringLiteral("compact")
                                : QStringLiteral("comfortable");
    const QVariantMap tokens = look == QStringLiteral("Custom")
                                   ? themeTokensFromText(configuration.value(QStringLiteral("themeTokens")).toString())
                                   : QVariantMap();
    return {{QStringLiteral("look"), look}, {QStringLiteral("density"), density}, {QStringLiteral("tokens"), tokens}};
}

} // namespace contribusage
```

In `src/core/CMakeLists.txt`, add `appearance.cpp appearance.h` at the top of the sources.

- [ ] **Step 4: Run it** — `cmake --build build && ctest --test-dir build --output-on-failure -R tst_appearance` → PASS.

- [ ] **Step 5: Commit** — format the files, `git add src/core tests`, commit `Core: appearance settings and validated theme tokens` with the co-author line.

---

### Task 2: The theme follows the look, density and tokens

**Files:** Modify `src/applet/Theme.qml`, `src/applet/popupmodel.h`, `src/applet/popupmodel.cpp`, `src/applet/main.qml`, `src/applet/config/main.xml`, `tests/applet/tst_appletqml.cpp`.

**Interfaces:** Produces `PopupModel.appearance` (`Q_PROPERTY(QVariantMap appearance READ appearance NOTIFY changed)`, from `appearanceFromMap` in `applySettings`); config keys `look` (String, `macOS`), `density` (String, `comfortable`), `themeTokens` (String, empty); `Theme` tokens become bindings.

- [ ] **Step 1: Write the failing tests** — add to `tst_appletqml.cpp`:
```cpp
    QObject *theme(QQmlEngine &engine) {
        return engine.singletonInstance<QObject *>(QString::fromLatin1(uri), QStringLiteral("Theme"));
    }

    void customTokensOverrideTheTheme() {
        QQmlEngine engine;
        QObject *popup = model(engine);
        QObject *style = theme(engine);
        QCOMPARE(style->property("accent").value<QColor>(), QColor(QStringLiteral("#0a84ff")));
        QMetaObject::invokeMethod(popup, "applySettings",
                                  Q_ARG(QVariantMap, (QVariantMap{{QStringLiteral("look"), QStringLiteral("Custom")},
                                                                  {QStringLiteral("themeTokens"), QStringLiteral(R"({"accent":"#112233","padding":20})")}})));
        QCOMPARE(style->property("accent").value<QColor>(), QColor(QStringLiteral("#112233")));
        QCOMPARE(style->property("padding").toInt(), 20);
        QMetaObject::invokeMethod(popup, "applySettings", Q_ARG(QVariantMap, QVariantMap()));
        QCOMPARE(style->property("accent").value<QColor>(), QColor(QStringLiteral("#0a84ff")));
    }

    // Review Focus 4.
    void plasmaLookFollowsThePalette() {
        QQmlEngine engine;
        QObject *popup = model(engine);
        QObject *style = theme(engine);
        QMetaObject::invokeMethod(popup, "applySettings",
                                  Q_ARG(QVariantMap, (QVariantMap{{QStringLiteral("look"), QStringLiteral("Plasma")}})));
        QCOMPARE(style->property("accent").value<QColor>(), QGuiApplication::palette().color(QPalette::Highlight));
        QMetaObject::invokeMethod(popup, "applySettings", Q_ARG(QVariantMap, QVariantMap()));
    }

    // Review Focus 5.
    void densityScalesSpacing() {
        QQmlEngine engine;
        QObject *popup = model(engine);
        QObject *style = theme(engine);
        const int padding = style->property("padding").toInt();
        QMetaObject::invokeMethod(popup, "applySettings",
                                  Q_ARG(QVariantMap, (QVariantMap{{QStringLiteral("density"), QStringLiteral("compact")}})));
        QVERIFY(style->property("padding").toInt() < padding);
        QVERIFY(style->property("spacing").toInt() < 8);
        QMetaObject::invokeMethod(popup, "applySettings", Q_ARG(QVariantMap, QVariantMap()));
    }
```
(and `#include <QGuiApplication>`, `#include <QPalette>`, `#include <QColor>` at the top.)

- [ ] **Step 2: Run them** — FAIL (`accent` stays `#0a84ff` / `appearance` missing).

- [ ] **Step 3: Implement**

`src/applet/config/main.xml` — add `look` (`macOS`), `density` (`comfortable`), `themeTokens` (empty) as String entries. `main.qml` — pass `"look": c.look, "density": c.density, "themeTokens": c.themeTokens` to `applySettings`.

`PopupModel` — `#include "core/appearance.h"`; member `QVariantMap m_appearance = appearanceFromMap({});`; `QVariantMap appearance() const { return m_appearance; }`; in `applySettings`, `m_appearance = appearanceFromMap(configuration);` before `Q_EMIT changed();`.

`src/applet/Theme.qml` — replace the constants with bindings (keep every name and every default value):
```qml
pragma Singleton
import QtQuick

// Every color and size the widget draws with (port design §3, layer 2). The look picks the base: macOS (the defaults
// below), Plasma (the system palette) or Custom (the defaults, with the user's token overrides). Density scales the
// spacing. Text colors come from Kirigami's theme in each component.
QtObject {
    id: theme

    readonly property var appearance: PopupModel.appearance ?? ({})
    readonly property string look: appearance.look ?? "macOS"
    readonly property var tokens: appearance.tokens ?? ({})
    readonly property real densityScale: appearance.density === "compact" ? 0.7 : 1
    property SystemPalette palette: SystemPalette { colorGroup: SystemPalette.Active }

    function token(name, fallback) {
        return tokens[name] !== undefined ? tokens[name] : fallback;
    }

    readonly property color accent: look === "Plasma" ? palette.highlight : token("accent", "#0a84ff")
    readonly property color warning: token("warning", "#ff9f0a")
    readonly property color critical: token("critical", "#ff453a")
    readonly property color claudeHue: token("claudeHue", "#ff9f0a")
    readonly property color githubHue: token("githubHue", "#30d158")

    // Opacities over the theme's text color.
    readonly property real secondaryOpacity: token("secondaryOpacity", 0.6)
    readonly property real trackOpacity: token("trackOpacity", 0.15)
    readonly property real emptyCellOpacity: token("emptyCellOpacity", 0.12)
    readonly property real dividerOpacity: token("dividerOpacity", 0.12)
    readonly property real mutedBarOpacity: token("mutedBarOpacity", 0.45)

    readonly property int popupWidth: token("popupWidth", 360)
    readonly property int padding: Math.round(token("padding", 12) * densityScale)
    readonly property int spacing: Math.round(token("spacing", 8) * densityScale)
    readonly property int meterHeight: token("meterHeight", 6)
    readonly property int cellGap: token("cellGap", 3)
    readonly property int weekChartHeight: Math.round(token("weekChartHeight", 32) * densityScale)
```
followed by the existing functions unchanged.

(If `PopupModel` cannot be referenced from the singleton because of import order, add `import plasma.applet.org.contribusage.widget` is not needed: both live in the same module; reference `PopupModel` directly. If `SystemPalette` cannot be a property of a `QtObject`, wrap it: `property var palette: Qt.createQmlObject("import QtQuick; SystemPalette {}", theme)`.)

- [ ] **Step 4: Run every test; render the popup with `look: Custom` and a red accent in the scratch renderer** → the session bar in the custom color.

- [ ] **Step 5: Commit** `Theme: looks, density and token overrides`.

---

### Task 3: QML overrides with fallback

**Files:** Modify `src/applet/popupmodel.h`, `src/applet/popupmodel.cpp`, `src/applet/FullPopup.qml`, `tests/applet/tst_appletqml.cpp`; create `llm-wiki/guides/qml-overrides.md`.

**Interfaces:** Produces `Q_INVOKABLE QUrl overrideUrl(const QString &block) const` (`file://…/contribusage/qml/<block>.qml` when it exists, else empty; `<block>` ∈ `ProviderGroup`, `HeatmapBlock`, `GitHubSection`, anything else empty), `Q_PROPERTY(QString overrideFolder READ overrideFolder CONSTANT)`. `FullPopup` loads `overrideUrl(name)` through a `Loader.source` with the same properties as the built-in block (`provider` + `insightsState`, `heatmap`, `github`), falls back to the built-in component when `status === Loader.Error`, and then shows "Custom <Block> failed to load; showing the built-in one." above it.

- [ ] **Step 1: Write the failing tests** — add to `tst_appletqml.cpp` (with `#include <QDir>`, `<QTemporaryDir>`):
```cpp
    // Overrides live in a folder the test controls: XDG_DATA_HOME is set before the model reads it.
    QString writeOverride(const QTemporaryDir &dir, const QString &block, const QByteArray &qml) {
        const QString folder = dir.filePath(QStringLiteral("contribusage/qml"));
        QDir().mkpath(folder);
        QFile file(folder + u'/' + block + QStringLiteral(".qml"));
        file.open(QIODevice::WriteOnly);
        file.write(qml);
        return file.fileName();
    }

    // Review Focus 3.
    void anOverrideReplacesTheBlock() {
        QTemporaryDir dir;
        qputenv("XDG_DATA_HOME", dir.path().toUtf8());
        writeOverride(dir, QStringLiteral("GitHubSection"),
                      "import QtQuick\nItem { objectName: \"customGitHub\"; property var github; implicitHeight: 10 }\n");
        QQmlEngine engine;
        QQmlComponent component(&engine, QUrl(QStringLiteral("qrc:/qt/qml/plasma/applet/org/contribusage/widget/FullPopup.qml")));
        std::unique_ptr<QObject> popup(component.create());
        QVERIFY2(popup, qPrintable(component.errorString()));
        QTRY_VERIFY(findItem(popup.get(), QStringLiteral("customGitHub")));
        qunsetenv("XDG_DATA_HOME");
    }

    // Review Focus 2.
    void aBrokenOverrideFallsBack() {
        QTemporaryDir dir;
        qputenv("XDG_DATA_HOME", dir.path().toUtf8());
        writeOverride(dir, QStringLiteral("GitHubSection"), "import QtQuick\nItem { this is not qml\n");
        QQmlEngine engine;
        QQmlComponent component(&engine, QUrl(QStringLiteral("qrc:/qt/qml/plasma/applet/org/contribusage/widget/FullPopup.qml")));
        std::unique_ptr<QObject> popup(component.create());
        QVERIFY2(popup, qPrintable(component.errorString()));
        QTRY_VERIFY(findItem(popup.get(), QStringLiteral("overrideNotice")));
        QVERIFY(findItem(popup.get(), QStringLiteral("tokenField")) || findItem(popup.get(), QStringLiteral("githubBuiltIn")));
        qunsetenv("XDG_DATA_HOME");
    }
```
(`GitHubSection.qml` gets `objectName: "githubBuiltIn"` on its root so the fallback is visible to the test.)

- [ ] **Step 2: Run them** → FAIL (`customGitHub` never appears).

- [ ] **Step 3: Implement**

`PopupModel`:
```cpp
QString PopupModel::overrideFolder() const {
    return QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + QStringLiteral("/contribusage/qml");
}

QUrl PopupModel::overrideUrl(const QString &block) const {
    static const QStringList blocks{QStringLiteral("ProviderGroup"), QStringLiteral("HeatmapBlock"),
                                    QStringLiteral("GitHubSection")};
    const QString file = overrideFolder() + u'/' + block + QStringLiteral(".qml");
    return blocks.contains(block) && QFileInfo::exists(file) ? QUrl::fromLocalFile(file) : QUrl();
}
```
(`#include <QFileInfo>`, `<QStandardPaths>`, `<QUrl>`; declare `Q_INVOKABLE QUrl overrideUrl(const QString &block) const;` and the `overrideFolder` property.)

`FullPopup.qml` — inside each block delegate, replace the single `Loader` with an override-aware pair:
```qml
            readonly property string blockName: block.modelData === "heatmap" ? "HeatmapBlock"
                : block.modelData === "github" ? "GitHubSection" : "ProviderGroup"
            readonly property url overrideSource: PopupModel.overrideUrl(blockName)
            property bool overrideFailed: false

            PlasmaComponents.Label {
                objectName: "overrideNotice"
                visible: block.overrideFailed
                text: "Custom " + block.blockName + " failed to load; showing the built-in one."
                font: Kirigami.Theme.smallFont
                opacity: Theme.secondaryOpacity
                wrapMode: Text.Wrap
                Layout.fillWidth: true
                Layout.margins: Theme.padding
            }
            Loader {
                id: overrideLoader
                active: block.hasData && block.overrideSource.toString().length > 0 && !block.overrideFailed
                visible: status === Loader.Ready
                Layout.fillWidth: true
                Layout.margins: Theme.padding
                onStatusChanged: if (status === Loader.Error) block.overrideFailed = true
                onLoaded: {
                    // The same documented properties as the built-in block (llm-wiki/guides/qml-overrides.md).
                    if (block.modelData === "heatmap") item.heatmap = Qt.binding(() => PopupModel.heatmap);
                    else if (block.modelData === "github") item.github = Qt.binding(() => PopupModel.github);
                    else item.provider = Qt.binding(() => PopupModel.providers[0]);
                }
                Component.onCompleted: if (active) setSource(block.overrideSource)
            }
            Loader {
                active: block.hasData && (block.overrideSource.toString().length === 0 || block.overrideFailed)
                Layout.fillWidth: true
                Layout.margins: Theme.padding
                sourceComponent: block.modelData === "heatmap" ? heatmapBlock
                    : block.modelData === "github" ? githubBlock : providerBlock
            }
```
(the divider `Rectangle` stays first). In `GitHubSection.qml`, add `objectName: "githubBuiltIn"` to the root.

`llm-wiki/guides/qml-overrides.md` (frontmatter `type: guide`, `status: active`, `updated:` today, `tracks: [src/applet/FullPopup.qml]`, `tags: [customization, qml]`): where the files go (`~/.local/share/contribusage/qml/<Block>.qml`), the blocks and their properties (`ProviderGroup`: `provider` map as from `limitsView` plus `activity`, `insights`, `sections`, and `insightsState`; `HeatmapBlock`: `heatmap` map with `layers`, `columns`, `cells`, `style`; `GitHubSection`: `github` map), the `Theme` singleton's tokens, the fallback and its notice, and the warning that an override is code that runs inside plasmashell with the user's rights. List it in `llm-wiki/index.md` under Guides.

- [ ] **Step 4: Run every test** → pass.

- [ ] **Step 5: Commit** `Applet: QML overrides for the popup's blocks, with fallback`.

---

### Task 4: The Appearance settings page

**Files:** Create `src/applet/ConfigAppearance.qml`; modify `src/applet/config/config.qml`, `src/applet/CMakeLists.txt`, `tests/applet/tst_appletqml.cpp` (add `"ConfigAppearance"` to `qmlFileCompiles_data` and to `configModelListsTheTabs`).

`src/applet/ConfigAppearance.qml`:
```qml
import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kcmutils as KCM
import org.kde.kirigami as Kirigami

// Port design §3: look, density, token overrides, and the folder for QML overrides.
KCM.SimpleKCM {
    property string cfg_look
    property string cfg_density
    property string cfg_themeTokens

    readonly property var looks: ["macOS", "Plasma", "Custom"]

    Kirigami.FormLayout {
        QQC2.ComboBox {
            Kirigami.FormData.label: "Look:"
            model: looks
            currentIndex: Math.max(0, looks.indexOf(cfg_look))
            onActivated: index => cfg_look = looks[index]
            Accessible.name: "Look"
        }
        QQC2.ComboBox {
            Kirigami.FormData.label: "Density:"
            model: ["Comfortable", "Compact"]
            currentIndex: cfg_density === "compact" ? 1 : 0
            onActivated: index => cfg_density = index === 1 ? "compact" : "comfortable"
            Accessible.name: "Density"
        }
        QQC2.TextArea {
            Kirigami.FormData.label: "Tokens (JSON):"
            enabled: cfg_look === "Custom"
            text: cfg_themeTokens
            placeholderText: '{"accent": "#0a84ff", "popupWidth": 400}'
            onEditingFinished: cfg_themeTokens = text
            Layout.preferredWidth: Kirigami.Units.gridUnit * 20
            Layout.preferredHeight: Kirigami.Units.gridUnit * 6
            Accessible.name: "Theme token overrides as JSON"
        }
        QQC2.Label {
            text: "Tokens: accent, warning, critical, claudeHue, githubHue (colors); secondaryOpacity, trackOpacity, emptyCellOpacity, dividerOpacity, mutedBarOpacity (0 to 1); popupWidth, padding, spacing, meterHeight, cellGap, weekChartHeight (pixels). Unknown or invalid values are ignored."
            wrapMode: Text.Wrap
            opacity: 0.7
            Layout.preferredWidth: Kirigami.Units.gridUnit * 20
        }
        Item { Kirigami.FormData.isSection: true }
        RowLayout {
            Kirigami.FormData.label: "QML overrides:"
            QQC2.Label { text: PopupModel.overrideFolder; elide: Text.ElideMiddle; Layout.maximumWidth: Kirigami.Units.gridUnit * 14 }
            QQC2.Button {
                text: "Open"
                onClicked: Qt.openUrlExternally("file://" + PopupModel.overrideFolder)
            }
        }
        QQC2.Label {
            text: "A file named ProviderGroup.qml, HeatmapBlock.qml or GitHubSection.qml there replaces that block. Overrides run as code inside Plasma; only use files you trust."
            wrapMode: Text.Wrap
            opacity: 0.7
            Layout.preferredWidth: Kirigami.Units.gridUnit * 20
        }
    }
}
```

`config/config.qml` — add after Popover:
```qml
    ConfigCategory {
        name: "Appearance"
        icon: "preferences-desktop-theme"
        source: "ConfigAppearance.qml"
    }
```

- [ ] Run every test (the page compiles; the model lists it); render the page in the scratch renderer; commit `Applet: the Appearance settings page`.

---

### Task 5: Contract and vault

- `SPEC.md` §17.8: replace the `L-6` line with `- [x] **L-6** Customization (port design §3): looks macOS, Plasma and Custom; theme tokens with validated overrides and density; QML overrides for ProviderGroup, HeatmapBlock and GitHubSection with fallback and notice (guide: llm-wiki/guides/qml-overrides.md); the Appearance settings page. Accept: \`tst_appearance\` and the theme and override tests in \`tst_appletqml\` pass.`
- `llm-wiki/modules/core.md` (`appearance.h`), `llm-wiki/modules/applet.md` (`appearance`, Theme bindings, `overrideUrl`, `overrideFolder`, ConfigAppearance), `llm-wiki/log.md` entry `## [YYYY-MM-DD] feat | Customization (L-6)` with two bullets.
- Run `llm-wiki/tools/wiki.sh lint && cmake --build build && QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure && clang-format --dry-run --Werror $(git ls-files '*.cpp' '*.h') && echo all-ok`; commit `Spec: L-6 done`.
