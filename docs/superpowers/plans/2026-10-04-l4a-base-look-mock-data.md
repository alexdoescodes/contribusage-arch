# L-4a: Base Look with Mock Data Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make the widget look like the macOS app: the panel label in the `prompt` style, and the popup with limits, activity, insights, the shared heatmap, GitHub and the footer. All of it is filled with the macOS app's preview data, so the look can be seen and checked before the providers exist.

**Architecture:** The formatting and heatmap rules are pure functions in `contribusage_core` (`src/core/display.*`, `src/core/heatmap.*`), each with a Qt Test. The applet gets a `PopupModel` QML singleton that builds the mock data through those functions, a `Theme` QML singleton holding every color and size, and one QML file per block. Later milestones replace `PopupModel`'s mock data with the refresh coordinator's state; the QML stays.

**Tech Stack:** C++23, Qt 6 (Core, Qml, Quick, Quick Shapes), Kirigami, Plasma components, Qt Test.

**Spec:** `docs/superpowers/specs/2026-10-04-linux-plasmoid-port-design.md` §3 (UI, look and feel). The look follows SPEC §11.1 to §11.5 and the macOS sources at `2603a21` (`App/Popover/*.swift`, `App/MenuBar/MenuBarArt.swift`) and `docs/assets/screenshot-dark.png`. The user chose to build this ahead of L-1 to L-3 (UI first), so the data is mock data until then.

## Global Constraints

- Everything from M0's Global Constraints still holds: C++23, `-Werror`, clang-format clean, `src/core/` links only Qt6::Core, nothing blocks the GUI thread, and the vault stays lint-clean.
- KDE's compile flags define `QT_NO_CAST_FROM_ASCII`: every string literal in C++ is `QStringLiteral(…)` or a `u'…'` character.
- Texts are English until the port designs translation (M0 ruling); they match the macOS app's English texts.
- Colors come only from `Theme.qml` or Kirigami's theme, never as literals in other QML files.
- The popup is 360 px wide (SPEC §11.2). Meter colors: below 70 % the accent, 70 to 89 % warning, 90 % and up critical (SPEC §11.4). The Claude Code hue is orange, GitHub's green, and heatmap levels 1 to 4 have opacity `0.2 + 0.2 × level` (SPEC §11.4).
- Mock data: the macOS app's `AppState+Mock.swift` values. Session 23 % resetting in 7,800 s; week (all models) 74 % resetting in 4 days; week (Opus) 93 %; 668 requests, 8 sessions; tokens per day for the last 7 days with cache read 1.1, 3.4, 2.0, 5.1, 4.4, 0.8 and 4.2 million plus input 12K, output 812K and cache write 300K; GitHub `octocat` with today 5, streak 12 and year 1,234.

## Review Focus

1. **A window past its reset (FR-11).** Expected: `–` instead of the percentage, and "reset, refreshing…". Pinned in Task 1 (`percentText` and `resetText` tests).
2. **Usage at or above 100 %.** Expected: the bar is full and doesn't overflow, the text reads `100%` (or the real value), and the color is critical. Pinned in Task 1 (`meterLevel` and `barFraction` tests).
3. **A week with no tokens at all.** Expected: all seven bars are empty, with no division by zero and no NaN heights. Pinned in Task 1 (`barFractions` zero case).
4. **A day without data versus a day with 0.** Expected: both draw the neutral cell, and the tooltip says "no data" for the first and "0 tokens" for the second. Pinned in Task 3 (`heatmapLineReadsNoData` test).
5. **A small or vertical panel.** Expected: the label scales with the panel height and stays readable; in a vertical panel it shows the glyph only. Pinned in Task 5, Step 4 (viewer checks in horizontal and vertical form factors).

---

## File Structure

| Path | Responsibility |
|---|---|
| `src/core/display.h`, `src/core/display.cpp` | SPEC §11.4 and §11.5: meter level, percent, reset, age and compact count texts, and bar fractions |
| `src/core/heatmap.h`, `src/core/heatmap.cpp` | FR-20 and FR-48: the 26-week day range and quartile levels |
| `src/core/CMakeLists.txt` | Adds the four files |
| `src/applet/popupmodel.h`, `src/applet/popupmodel.cpp` | `PopupModel` singleton: what QML shows, built from mock data through the core functions |
| `src/applet/Theme.qml` | `Theme` singleton: colors, sizes, opacities |
| `src/applet/SectionHeader.qml`, `LimitBar.qml`, `ActivitySection.qml`, `InsightsSection.qml`, `ProviderGroup.qml`, `HeatmapBlock.qml`, `GitHubSection.qml`, `Footer.qml`, `FullPopup.qml` | The popup's blocks |
| `src/applet/PromptGlyph.qml`, `src/applet/CompactLabel.qml` | The panel label in the `prompt` style |
| `src/applet/main.qml` | Uses `CompactLabel` and `FullPopup` |
| `src/applet/CMakeLists.txt` | Adds the files; marks `Theme.qml` a singleton |
| `tests/core/tst_display.cpp`, `tests/core/tst_heatmap.cpp` | Unit tests for the core functions |
| `tests/applet/tst_appletqml.cpp` | Compiles every QML file; checks `PopupModel` |
| `tests/CMakeLists.txt` | Registers the new tests |

---

### Task 1: Display texts in the core

**Files:**
- Create: `src/core/display.h`, `src/core/display.cpp`, `tests/core/tst_display.cpp`
- Modify: `src/core/CMakeLists.txt`, `tests/CMakeLists.txt`

**Interfaces:**
- Produces (namespace `contribusage`, header `core/display.h`):
  - `enum class MeterLevel { Normal, Warning, Critical };`
  - `MeterLevel meterLevel(double usedPercent);`
  - `double barFraction(double usedPercent);` (0 to 1, clamped)
  - `QList<double> barFractions(const QList<qint64> &values);` (each divided by the maximum; all 0 when the maximum is 0)
  - `QString percentText(double usedPercent, bool isBelowOne, const std::optional<QDateTime> &resetsAt, const QDateTime &now);`
  - `std::optional<QString> resetText(const std::optional<QDateTime> &resetsAt, const QDateTime &now, const QLocale &locale = QLocale());`
  - `QString ageText(const QDateTime &fetchedAt, const QDateTime &now);`
  - `QString compactCount(qint64 value);`

- [ ] **Step 1: Write the failing test**

`tests/core/tst_display.cpp`:
```cpp
#include "core/display.h"

#include <QTest>

using namespace contribusage;

// Sunday 2026-10-04, 09:41 local time, as in the macOS screenshot.
static const QDateTime now(QDate(2026, 10, 4), QTime(9, 41));

class TestDisplay : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void meterLevelFollowsTheThresholds() {
        QCOMPARE(meterLevel(69.9), MeterLevel::Normal);
        QCOMPARE(meterLevel(70), MeterLevel::Warning);
        QCOMPARE(meterLevel(89.9), MeterLevel::Warning);
        QCOMPARE(meterLevel(90), MeterLevel::Critical);
        QCOMPARE(meterLevel(140), MeterLevel::Critical);
    }

    void barFractionIsClamped() {
        QCOMPARE(barFraction(23), 0.23);
        QCOMPARE(barFraction(140), 1.0);
        QCOMPARE(barFraction(-5), 0.0);
    }

    void barFractionsScaleToTheBusiestDay() {
        QCOMPARE(barFractions({1, 2, 4}), (QList<double>{0.25, 0.5, 1.0}));
        QCOMPARE(barFractions({0, 0, 0}), (QList<double>{0.0, 0.0, 0.0}));
        QCOMPARE(barFractions({}), QList<double>{});
    }

    void percentText_data() {
        QTest::addColumn<double>("used");
        QTest::addColumn<bool>("belowOne");
        QTest::addColumn<qint64>("resetsIn");
        QTest::addColumn<QString>("expected");
        QTest::newRow("value") << 23.7 << false << qint64(7800) << QStringLiteral("23%");
        QTest::newRow("full") << 100.0 << false << qint64(7800) << QStringLiteral("100%");
        QTest::newRow("below one") << 0.4 << true << qint64(7800) << QStringLiteral("<1%");
        QTest::newRow("reset passed") << 97.0 << false << qint64(-60) << QStringLiteral("–");
    }
    void percentText() {
        QFETCH(double, used);
        QFETCH(bool, belowOne);
        QFETCH(qint64, resetsIn);
        QFETCH(QString, expected);
        QCOMPARE(contribusage::percentText(used, belowOne, now.addSecs(resetsIn), now), expected);
    }

    void percentTextWithoutResetTime() { QCOMPARE(contribusage::percentText(41, false, std::nullopt, now), u"41%"); }

    void resetText_data() {
        QTest::addColumn<qint64>("resetsIn");
        QTest::addColumn<QString>("expected");
        QTest::newRow("hours and minutes") << qint64(7800) << QStringLiteral("resets in 2 hr 10 min");
        QTest::newRow("whole hour") << qint64(3600) << QStringLiteral("resets in 1 hr");
        QTest::newRow("minutes") << qint64(720) << QStringLiteral("resets in 12 min");
        QTest::newRow("never below one minute") << qint64(20) << QStringLiteral("resets in 1 min");
        QTest::newRow("exactly a day") << qint64(86400) << QStringLiteral("resets in 24 hr");
        QTest::newRow("more than a day") << qint64(4 * 86400 + 4 * 3600 + 17 * 60)
                                         << QStringLiteral("resets Thu 13:58");
        QTest::newRow("passed") << qint64(0) << QStringLiteral("reset, refreshing…");
    }
    void resetText() {
        QFETCH(qint64, resetsIn);
        QFETCH(QString, expected);
        QCOMPARE(contribusage::resetText(now.addSecs(resetsIn), now, QLocale::c()), std::optional<QString>(expected));
    }

    void resetTextWithoutResetTime() { QCOMPARE(contribusage::resetText(std::nullopt, now), std::nullopt); }

    void ageText_data() {
        QTest::addColumn<qint64>("age");
        QTest::addColumn<QString>("expected");
        QTest::newRow("just now") << qint64(59) << QStringLiteral("updated just now");
        QTest::newRow("minutes") << qint64(120) << QStringLiteral("updated 2 min ago");
        QTest::newRow("hours") << qint64(3 * 3600 + 59 * 60) << QStringLiteral("updated 3 hr ago");
        QTest::newRow("one day") << qint64(86400) << QStringLiteral("updated 1 day ago");
        QTest::newRow("days") << qint64(3 * 86400) << QStringLiteral("updated 3 days ago");
    }
    void ageText() {
        QFETCH(qint64, age);
        QFETCH(QString, expected);
        QCOMPARE(contribusage::ageText(now.addSecs(-age), now), expected);
    }

    void compactCount_data() {
        QTest::addColumn<qint64>("value");
        QTest::addColumn<QString>("expected");
        QTest::newRow("small") << qint64(668) << QStringLiteral("668");
        QTest::newRow("thousand, one decimal") << qint64(1234) << QStringLiteral("1.2K");
        QTest::newRow("thousand, no zero decimal") << qint64(2000) << QStringLiteral("2K");
        QTest::newRow("tens of thousands") << qint64(12000) << QStringLiteral("12K");
        QTest::newRow("hundreds of thousands") << qint64(812000) << QStringLiteral("812K");
        QTest::newRow("millions") << qint64(5324000) << QStringLiteral("5.3M");
        QTest::newRow("tens of millions") << qint64(41000000) << QStringLiteral("41M");
    }
    void compactCount() {
        QFETCH(qint64, value);
        QFETCH(QString, expected);
        QCOMPARE(contribusage::compactCount(value), expected);
    }
};

QTEST_GUILESS_MAIN(TestDisplay)
#include "tst_display.moc"
```

Append to `tests/CMakeLists.txt`:
```cmake

ecm_add_test(core/tst_display.cpp
    TEST_NAME tst_display
    LINK_LIBRARIES contribusage_core Qt6::Test)
```

- [ ] **Step 2: Run it and confirm it fails**

Run: `cmake -B build -G Ninja && cmake --build build`
Expected: FAIL with `core/display.h: No such file or directory`.

- [ ] **Step 3: Write the implementation**

`src/core/display.h`:
```cpp
#pragma once

#include <QDateTime>
#include <QList>
#include <QLocale>
#include <QString>

#include <optional>

namespace contribusage {

// SPEC §11.4: the meter color's level for a used percentage.
enum class MeterLevel { Normal, Warning, Critical };
MeterLevel meterLevel(double usedPercent);

// How much of a meter is filled, 0 to 1: usage can exceed 100 %, a bar can't.
double barFraction(double usedPercent);

// Bar heights relative to the largest value; all 0 when nothing happened.
QList<double> barFractions(const QList<qint64> &values);

// SPEC §11.5, FR-11: "23%", "<1%", or "–" once the window has reset.
QString percentText(double usedPercent, bool isBelowOne, const std::optional<QDateTime> &resetsAt,
                    const QDateTime &now);

// SPEC §11.5: "resets in 2 hr 10 min", "resets Thu 1:58 PM", "reset, refreshing…"; nullopt without a reset time.
std::optional<QString> resetText(const std::optional<QDateTime> &resetsAt, const QDateTime &now,
                                 const QLocale &locale = QLocale());

// SPEC §11.5: "updated just now", "updated 2 min ago", "updated 3 hr ago", "updated 2 days ago".
QString ageText(const QDateTime &fetchedAt, const QDateTime &now);

// SPEC §11.5: compact counts, "668", "1.2K", "812K", "4.2M".
QString compactCount(qint64 value);

} // namespace contribusage
```

`src/core/display.cpp`:
```cpp
#include "core/display.h"

#include <QStringList>

#include <algorithm>
#include <cmath>

namespace contribusage {

namespace {

bool isReset(const std::optional<QDateTime> &resetsAt, const QDateTime &now) {
    return resetsAt && *resetsAt <= now;
}

} // namespace

MeterLevel meterLevel(double usedPercent) {
    if (usedPercent >= 90) {
        return MeterLevel::Critical;
    }
    return usedPercent >= 70 ? MeterLevel::Warning : MeterLevel::Normal;
}

double barFraction(double usedPercent) {
    return std::clamp(usedPercent / 100.0, 0.0, 1.0);
}

QList<double> barFractions(const QList<qint64> &values) {
    const qint64 highest = values.isEmpty() ? 0 : *std::max_element(values.cbegin(), values.cend());
    QList<double> fractions;
    fractions.reserve(values.size());
    for (const qint64 value : values) {
        fractions.append(highest > 0 ? double(value) / double(highest) : 0.0);
    }
    return fractions;
}

QString percentText(double usedPercent, bool isBelowOne, const std::optional<QDateTime> &resetsAt,
                    const QDateTime &now) {
    if (isReset(resetsAt, now)) {
        return QStringLiteral("–");
    }
    if (isBelowOne) {
        return QStringLiteral("<1%");
    }
    return QString::number(static_cast<int>(usedPercent)) + u'%';
}

std::optional<QString> resetText(const std::optional<QDateTime> &resetsAt, const QDateTime &now,
                                 const QLocale &locale) {
    if (!resetsAt) {
        return std::nullopt;
    }
    const qint64 remaining = now.secsTo(*resetsAt);
    if (remaining <= 0) {
        return QStringLiteral("reset, refreshing…");
    }
    if (remaining > 24 * 3600) {
        const QDateTime local = resetsAt->toLocalTime();
        return QStringLiteral("resets ") + locale.toString(local.date(), QStringLiteral("ddd")) + u' ' +
               locale.toString(local.time(), QLocale::ShortFormat);
    }
    const qint64 minutes = std::max<qint64>(remaining, 60) / 60; // never "0 min"
    QStringList parts;
    if (minutes >= 60) {
        parts.append(QString::number(minutes / 60) + QStringLiteral(" hr"));
    }
    if (minutes % 60 > 0) {
        parts.append(QString::number(minutes % 60) + QStringLiteral(" min"));
    }
    return QStringLiteral("resets in ") + parts.join(u' ');
}

QString ageText(const QDateTime &fetchedAt, const QDateTime &now) {
    const qint64 age = fetchedAt.secsTo(now);
    if (age < 60) {
        return QStringLiteral("updated just now");
    }
    QString unit;
    if (age >= 86400) {
        const qint64 days = age / 86400;
        unit = QString::number(days) + (days == 1 ? QStringLiteral(" day") : QStringLiteral(" days"));
    } else if (age >= 3600) {
        unit = QString::number(age / 3600) + QStringLiteral(" hr");
    } else {
        unit = QString::number(age / 60) + QStringLiteral(" min");
    }
    return QStringLiteral("updated ") + unit + QStringLiteral(" ago");
}

QString compactCount(qint64 value) {
    struct Unit {
        qint64 size;
        char16_t suffix;
    };
    static constexpr Unit units[] = {{1'000'000'000, u'B'}, {1'000'000, u'M'}, {1'000, u'K'}};
    for (const Unit &unit : units) {
        if (value < unit.size) {
            continue;
        }
        const double scaled = double(value) / double(unit.size);
        const double tenths = std::round(scaled * 10) / 10;
        if (tenths < 10) {
            QString text = QString::number(tenths, 'f', 1);
            if (text.endsWith(QStringLiteral(".0"))) {
                text.chop(2);
            }
            return text + QChar(unit.suffix);
        }
        return QString::number(std::llround(scaled)) + QChar(unit.suffix);
    }
    return QString::number(value);
}

} // namespace contribusage
```

In `src/core/CMakeLists.txt`, change the source list of `add_library` to:
```cmake
add_library(contribusage_core STATIC
    display.cpp
    display.h
    version.cpp
    version.h)
```

- [ ] **Step 4: Run the tests and confirm they pass**

Run: `cmake --build build && ctest --test-dir build --output-on-failure -R tst_display`
Expected: PASS. If the `more than a day` row fails only because of the time zone, the test machine's local zone moved the reset across midnight. The test builds both datetimes in local time, so it must not: investigate rather than change the expected text.

- [ ] **Step 5: Format and commit**

Run: `clang-format -i src/core/display.* tests/core/tst_display.cpp && clang-format --dry-run --Werror $(git ls-files -co --exclude-standard '*.cpp' '*.h') && cmake --build build && QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure`
Expected: no format output; all tests pass.

```bash
git add src/core tests
git commit -m "Core: display texts for meters, resets, ages and counts

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 2: Heatmap days and levels in the core

**Files:**
- Create: `src/core/heatmap.h`, `src/core/heatmap.cpp`, `tests/core/tst_heatmap.cpp`
- Modify: `src/core/CMakeLists.txt`, `tests/CMakeLists.txt`

**Interfaces:**
- Produces (namespace `contribusage`, header `core/heatmap.h`):
  - `QList<QDate> heatmapDays(QDate today);` (oldest first; the first is a Sunday, the last `today`)
  - `QHash<QDate, int> quartileLevels(const QHash<QDate, qint64> &values);`

- [ ] **Step 1: Write the failing test**

`tests/core/tst_heatmap.cpp`:
```cpp
#include "core/heatmap.h"

#include <QTest>

using namespace contribusage;

class TestHeatmap : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void daysOnASundayAreTwentyFiveWeeksAndOneDay() {
        const QDate sunday(2026, 10, 4);
        const auto days = heatmapDays(sunday);
        QCOMPARE(days.size(), 25 * 7 + 1);
        QCOMPARE(days.last(), sunday);
        QCOMPARE(days.first(), QDate(2026, 4, 12));
        QCOMPARE(days.first().dayOfWeek(), Qt::Sunday);
    }

    void daysOnAThursdayFillTheLastWeekThroughToday() {
        const QDate thursday(2026, 10, 8);
        const auto days = heatmapDays(thursday);
        QCOMPARE(days.size(), 25 * 7 + 5);
        QCOMPARE(days.last(), thursday);
        QCOMPARE(days.first().dayOfWeek(), Qt::Sunday);
    }

    void levelsAreQuartilesOfTheActiveDays() {
        const QDate day(2026, 10, 1);
        const QHash<QDate, qint64> values{
            {day, 0}, {day.addDays(1), 10}, {day.addDays(2), 20}, {day.addDays(3), 30}, {day.addDays(4), 40}};
        const auto levels = quartileLevels(values);
        QCOMPARE(levels.value(day), 0);
        QCOMPARE(levels.value(day.addDays(1)), 1);
        QCOMPARE(levels.value(day.addDays(2)), 2);
        QCOMPARE(levels.value(day.addDays(3)), 3);
        QCOMPARE(levels.value(day.addDays(4)), 4);
    }

    void aSingleActiveDayIsTheBusiest() {
        const QDate day(2026, 10, 1);
        QCOMPARE(quartileLevels({{day, 5}}).value(day), 4);
    }

    void noValuesNoLevels() { QVERIFY(quartileLevels({}).isEmpty()); }
};

QTEST_GUILESS_MAIN(TestHeatmap)
#include "tst_heatmap.moc"
```

Append to `tests/CMakeLists.txt`:
```cmake

ecm_add_test(core/tst_heatmap.cpp
    TEST_NAME tst_heatmap
    LINK_LIBRARIES contribusage_core Qt6::Test)
```

- [ ] **Step 2: Run it and confirm it fails**

Run: `cmake -B build -G Ninja && cmake --build build`
Expected: FAIL with `core/heatmap.h: No such file or directory`.

- [ ] **Step 3: Write the implementation**

`src/core/heatmap.h`:
```cpp
#pragma once

#include <QDate>
#include <QHash>
#include <QList>

namespace contribusage {

// FR-20, FR-48: 26 week columns from Sunday, as GitHub's, the last one ending with `today`; oldest first.
QList<QDate> heatmapDays(QDate today);

// FR-48: a day's level, 0 to 4: 0 without tokens, otherwise its quartile among the non-zero values, so the busiest
// day is 4. `values` holds the shown range only.
QHash<QDate, int> quartileLevels(const QHash<QDate, qint64> &values);

} // namespace contribusage
```

`src/core/heatmap.cpp`:
```cpp
#include "core/heatmap.h"

#include <algorithm>

namespace contribusage {

QList<QDate> heatmapDays(QDate today) {
    const int weekdayFromSunday = today.dayOfWeek() % 7 + 1; // Qt counts Monday 1 to Sunday 7
    const int count = 25 * 7 + weekdayFromSunday;
    QList<QDate> days;
    days.reserve(count);
    for (int offset = count - 1; offset >= 0; --offset) {
        days.append(today.addDays(-offset));
    }
    return days;
}

QHash<QDate, int> quartileLevels(const QHash<QDate, qint64> &values) {
    QList<qint64> active;
    for (const qint64 value : values) {
        if (value > 0) {
            active.append(value);
        }
    }
    std::sort(active.begin(), active.end());
    QHash<QDate, int> levels;
    for (auto it = values.cbegin(); it != values.cend(); ++it) {
        if (it.value() <= 0) {
            levels.insert(it.key(), 0);
            continue;
        }
        const qsizetype atMost = std::upper_bound(active.cbegin(), active.cend(), it.value()) - active.cbegin();
        levels.insert(it.key(), int((4 * atMost + active.size() - 1) / active.size()));
    }
    return levels;
}

} // namespace contribusage
```

In `src/core/CMakeLists.txt`, add `heatmap.cpp` and `heatmap.h` to the `add_library` list, keeping it alphabetical:
```cmake
add_library(contribusage_core STATIC
    display.cpp
    display.h
    heatmap.cpp
    heatmap.h
    version.cpp
    version.h)
```

- [ ] **Step 4: Run the tests and confirm they pass**

Run: `cmake --build build && ctest --test-dir build --output-on-failure -R tst_heatmap`
Expected: PASS.

- [ ] **Step 5: Format and commit**

Run: `clang-format -i src/core/heatmap.* tests/core/tst_heatmap.cpp && clang-format --dry-run --Werror $(git ls-files -co --exclude-standard '*.cpp' '*.h') && cmake --build build && QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure`
Expected: no format output; all tests pass.

```bash
git add src/core tests
git commit -m "Core: heatmap day range and quartile levels

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 3: The popup model with mock data, and the theme

**Files:**
- Create: `src/applet/popupmodel.h`, `src/applet/popupmodel.cpp`, `src/applet/Theme.qml`
- Modify: `src/applet/CMakeLists.txt`, `tests/applet/tst_appletqml.cpp`, `tests/CMakeLists.txt`

**Interfaces:**
- Consumes: `core/display.h` and `core/heatmap.h` (Tasks 1 and 2).
- Produces:
  - QML singleton `PopupModel`, with these `CONSTANT` properties:
    - `providers: list<map>`. Each provider has `name`, `symbol` (an icon name), and `updatedText`.
      - `windows: list<map>`, each with `label`, `percentText`, `fraction` (0 to 1), `level` (`"normal"`, `"warning"` or `"critical"`) and `resetText`.
      - `activity: map` with `todayText`, `categoriesText` and `week` (7 maps of `fraction` and `today`, a bool).
      - `insights: map` with `note` and `periods` (maps of `label`, `summary`, `rows` (maps of `label` and `percent`) and `rankings` (maps of `title` and `rows`)).
    - `heatmap: map` with `layers` (maps of `name` and `hue`, which is `"claude"` or `"github"`), `columns` (an int) and `cells`. Each cell has `line` (the tooltip) and `stripes` (maps of `hue` and `level`, 1 to 4; empty for the neutral cell).
    - `github: map` with `login`, `statsText` and `updatedText`.
    - `label: map` with `text`, `fraction` and `level`.
  - QML singleton `Theme` with the properties named in Step 3.

- [ ] **Step 1: Write the failing tests**

In `tests/applet/tst_appletqml.cpp`, add `#include "core/display.h"` and `#include "core/heatmap.h"` at the top. Then add these slots before the closing `};` of the class:
```cpp
    void popupModelHoldsTheMockLimits() {
        QQmlEngine engine;
        auto *model = engine.singletonInstance<QObject *>(QString::fromLatin1(uri), QStringLiteral("PopupModel"));
        QVERIFY(model);
        const auto providers = model->property("providers").toList();
        QCOMPARE(providers.size(), 1);
        const auto claude = providers.first().toMap();
        QCOMPARE(claude.value(QStringLiteral("name")).toString(), QStringLiteral("Claude Code"));
        QCOMPARE(claude.value(QStringLiteral("updatedText")).toString(), QStringLiteral("updated 2 min ago"));
        const auto windows = claude.value(QStringLiteral("windows")).toList();
        QCOMPARE(windows.size(), 3);
        const auto session = windows.at(0).toMap();
        QCOMPARE(session.value(QStringLiteral("percentText")).toString(), QStringLiteral("23%"));
        QCOMPARE(session.value(QStringLiteral("level")).toString(), QStringLiteral("normal"));
        QCOMPARE(session.value(QStringLiteral("resetText")).toString(), QStringLiteral("resets in 2 hr 10 min"));
        QCOMPARE(windows.at(1).toMap().value(QStringLiteral("level")).toString(), QStringLiteral("warning"));
        QCOMPARE(windows.at(2).toMap().value(QStringLiteral("level")).toString(), QStringLiteral("critical"));
    }

    void popupModelHoldsTheMockActivity() {
        QQmlEngine engine;
        auto *model = engine.singletonInstance<QObject *>(QString::fromLatin1(uri), QStringLiteral("PopupModel"));
        const auto activity =
            model->property("providers").toList().first().toMap().value(QStringLiteral("activity")).toMap();
        QCOMPARE(activity.value(QStringLiteral("todayText")).toString(),
                 QStringLiteral("Today: 668 requests · 8 sessions · 5.3M"));
        QCOMPARE(activity.value(QStringLiteral("categoriesText")).toString(),
                 QStringLiteral("Input 12K · Output 812K · Cache read 4.2M · Cache write 300K"));
        const auto week = activity.value(QStringLiteral("week")).toList();
        QCOMPARE(week.size(), 7);
        QVERIFY(week.last().toMap().value(QStringLiteral("today")).toBool());
    }

    void popupModelHeatmapCoversTheShownRange() {
        QQmlEngine engine;
        auto *model = engine.singletonInstance<QObject *>(QString::fromLatin1(uri), QStringLiteral("PopupModel"));
        const auto heatmap = model->property("heatmap").toMap();
        const auto days = contribusage::heatmapDays(QDate::currentDate());
        QCOMPARE(heatmap.value(QStringLiteral("cells")).toList().size(), days.size());
        QCOMPARE(heatmap.value(QStringLiteral("columns")).toInt(), (days.size() + 6) / 7);
        QCOMPARE(heatmap.value(QStringLiteral("layers")).toList().size(), 2);
        const auto today = heatmap.value(QStringLiteral("cells")).toList().last().toMap();
        QVERIFY(today.value(QStringLiteral("line")).toString().startsWith(QDate::currentDate().toString(Qt::ISODate)));
    }

    void heatmapLineReadsNoData() {
        QQmlEngine engine;
        auto *model = engine.singletonInstance<QObject *>(QString::fromLatin1(uri), QStringLiteral("PopupModel"));
        // The mock Claude Code layer starts 120 days ago (FR-48: earlier days are "no data").
        const auto first = model->property("heatmap").toMap().value(QStringLiteral("cells")).toList().first().toMap();
        QVERIFY2(first.value(QStringLiteral("line")).toString().contains(QStringLiteral("Claude Code: no data")),
                 qPrintable(first.value(QStringLiteral("line")).toString()));
    }

    void popupModelLabelShowsTheSession() {
        QQmlEngine engine;
        auto *model = engine.singletonInstance<QObject *>(QString::fromLatin1(uri), QStringLiteral("PopupModel"));
        const auto label = model->property("label").toMap();
        QCOMPARE(label.value(QStringLiteral("text")).toString(), QStringLiteral("23%"));
        QCOMPARE(label.value(QStringLiteral("fraction")).toDouble(), 0.23);
    }

    void themeSingletonResolves() {
        QQmlEngine engine;
        QQmlComponent component(&engine);
        component.setData("import QtQuick\nimport plasma.applet.org.contribusage.widget\nQtObject { "
                          "property int width: Theme.popupWidth }",
                          QUrl());
        std::unique_ptr<QObject> object(component.create());
        QVERIFY2(object, qPrintable(component.errorString()));
        QCOMPARE(object->property("width").toInt(), 360);
    }
```

Also add `#include <memory>` at the top (for `std::unique_ptr`). In `tests/CMakeLists.txt`, make `tst_appletqml` link the core library: change its `LINK_LIBRARIES Qt6::Qml Qt6::Gui Qt6::Test` to `LINK_LIBRARIES contribusage_core Qt6::Qml Qt6::Gui Qt6::Test`.

- [ ] **Step 2: Run it and confirm it fails**

Run: `cmake -B build -G Ninja && cmake --build build && ctest --test-dir build --output-on-failure -R tst_appletqml`
Expected: FAIL. `popupModel…` fails with `'model' returned FALSE`, and `themeSingletonResolves` with `Theme is not a type` or `ReferenceError: Theme is not defined`.

- [ ] **Step 3: Write the theme and the model**

`src/applet/Theme.qml`:
```qml
pragma Singleton
import QtQuick

// Every color and size the widget draws with (port design §3, layer 2). The defaults are the macOS app's look; the
// customization layers (L-6) override these properties. Text colors come from Kirigami's theme in each component.
QtObject {
    readonly property color accent: "#0a84ff"
    readonly property color warning: "#ff9f0a"
    readonly property color critical: "#ff453a"
    readonly property color claudeHue: "#ff9f0a"
    readonly property color githubHue: "#30d158"

    // Opacities over the theme's text color.
    readonly property real secondaryOpacity: 0.6
    readonly property real trackOpacity: 0.15
    readonly property real emptyCellOpacity: 0.12
    readonly property real dividerOpacity: 0.12
    readonly property real mutedBarOpacity: 0.45

    readonly property int popupWidth: 360
    readonly property int padding: 12
    readonly property int spacing: 8
    readonly property int meterHeight: 6
    readonly property int cellGap: 3
    readonly property int weekChartHeight: 32

    function meterColor(level) {
        return level === "critical" ? critical : level === "warning" ? warning : accent;
    }

    function hue(name) {
        return name === "github" ? githubHue : claudeHue;
    }

    // SPEC §11.4: levels 1 to 4 from 40 % to full strength.
    function levelOpacity(level) {
        return 0.2 + 0.2 * level;
    }
}
```

`src/applet/popupmodel.h`:
```cpp
#pragma once

#include <QObject>
#include <QVariantList>
#include <QVariantMap>
#include <QtQml/qqmlregistration.h>

// What the popup and the panel label show. Until the providers land (L-1 to L-3) it holds the macOS app's preview
// data, built through the same core functions the real data will use.
class PopupModel : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
    Q_PROPERTY(QVariantList providers READ providers CONSTANT)
    Q_PROPERTY(QVariantMap heatmap READ heatmap CONSTANT)
    Q_PROPERTY(QVariantMap github READ github CONSTANT)
    Q_PROPERTY(QVariantMap label READ label CONSTANT)

public:
    explicit PopupModel(QObject *parent = nullptr);

    QVariantList providers() const { return m_providers; }
    QVariantMap heatmap() const { return m_heatmap; }
    QVariantMap github() const { return m_github; }
    QVariantMap label() const { return m_label; }

private:
    QVariantList m_providers;
    QVariantMap m_heatmap;
    QVariantMap m_github;
    QVariantMap m_label;
};
```

`src/applet/popupmodel.cpp`:
```cpp
#include "popupmodel.h"

#include "core/display.h"
#include "core/heatmap.h"

#include <QLocale>

#include <algorithm>
#include <initializer_list>
#include <utility>

using namespace contribusage;

namespace {

QString levelName(MeterLevel level) {
    switch (level) {
    case MeterLevel::Critical:
        return QStringLiteral("critical");
    case MeterLevel::Warning:
        return QStringLiteral("warning");
    case MeterLevel::Normal:
        break;
    }
    return QStringLiteral("normal");
}

QVariantMap window(const QString &label, double usedPercent, qint64 resetsIn, const QDateTime &now) {
    const std::optional<QDateTime> resetsAt = now.addSecs(resetsIn);
    return {
        {QStringLiteral("label"), label},
        {QStringLiteral("percentText"), percentText(usedPercent, false, resetsAt, now)},
        {QStringLiteral("fraction"), barFraction(usedPercent)},
        {QStringLiteral("level"), levelName(meterLevel(usedPercent))},
        {QStringLiteral("resetText"), resetText(resetsAt, now).value_or(QString())},
    };
}

QVariantMap share(const QString &label, int percent) {
    return {{QStringLiteral("label"), label}, {QStringLiteral("percent"), QString::number(percent) + u'%'}};
}

QVariantList shares(std::initializer_list<std::pair<QString, int>> items) {
    QVariantList list;
    for (const auto &[label, percent] : items) {
        list.append(share(label, percent));
    }
    return list;
}

QVariantMap insights() {
    const QVariantMap day{
        {QStringLiteral("label"), QStringLiteral("Last 24h")},
        {QStringLiteral("summary"), QStringLiteral("206 requests · 5 sessions")},
        {QStringLiteral("rows"), shares({{QStringLiteral("At >150k context"), 62}})},
        {QStringLiteral("rankings"),
         QVariantList{
             QVariantMap{{QStringLiteral("title"), QStringLiteral("Skills")},
                         {QStringLiteral("rows"), shares({{QStringLiteral("/skill-a"), 35},
                                                          {QStringLiteral("/skill-b"), 23},
                                                          {QStringLiteral("/skill-c"), 21}})}},
             QVariantMap{{QStringLiteral("title"), QStringLiteral("Plugins")},
                         {QStringLiteral("rows"), shares({{QStringLiteral("plugin-a"), 56}})}},
         }},
    };
    const QVariantMap week{
        {QStringLiteral("label"), QStringLiteral("Last 7d")},
        {QStringLiteral("summary"), QStringLiteral("3350 requests · 88 sessions")},
        {QStringLiteral("rows"),
         shares({{QStringLiteral("At >150k context"), 54}, {QStringLiteral("Subagent-heavy sessions"), 13}})},
        {QStringLiteral("rankings"),
         QVariantList{QVariantMap{{QStringLiteral("title"), QStringLiteral("Skills")},
                                  {QStringLiteral("rows"), shares({{QStringLiteral("/skill-a"), 42}})}}}},
    };
    return {{QStringLiteral("note"), QStringLiteral("Approximate, based on local sessions on this machine.")},
            {QStringLiteral("periods"), QVariantList{day, week}}};
}

// The macOS preview's tokens per day for the last 7 days (cache read in millions, plus input, output and cache write),
// and a repeating pattern for the 113 days before them, so the heatmap has history. Older days have no data.
QHash<QDate, qint64> mockTokens(QDate today) {
    static constexpr double lastWeek[] = {1.1, 3.4, 2.0, 5.1, 4.4, 0.8, 4.2};
    constexpr qint64 otherTokens = 12'000 + 812'000 + 300'000;
    QHash<QDate, qint64> tokens;
    for (int index = 0; index < 120; ++index) {
        const QDate day = today.addDays(index - 119);
        const int fromEnd = 119 - index;
        if (fromEnd < 7) {
            tokens.insert(day, qint64(lastWeek[6 - fromEnd] * 1'000'000) + otherTokens);
        } else {
            const int step = (index * 37 + index / 5) % 11;
            tokens.insert(day, step == 0 ? 0 : step * 500'000);
        }
    }
    return tokens;
}

// The macOS preview's GitHub calendar: 26 weeks of a fixed pattern ending today.
QHash<QDate, int> mockContributions(QDate today) {
    QHash<QDate, int> counts;
    for (int index = 0; index < 182; ++index) {
        counts.insert(today.addDays(index - 181), (index * 7 + index / 3) % 9);
    }
    return counts;
}

} // namespace

PopupModel::PopupModel(QObject *parent) : QObject(parent) {
    const QDateTime now = QDateTime::currentDateTime();
    const QDate today = now.date();
    const auto tokens = mockTokens(today);

    QList<qint64> week;
    for (int offset = 6; offset >= 0; --offset) {
        week.append(tokens.value(today.addDays(-offset)));
    }
    QVariantList weekBars;
    const auto fractions = barFractions(week);
    for (qsizetype index = 0; index < fractions.size(); ++index) {
        weekBars.append(QVariantMap{{QStringLiteral("fraction"), fractions.at(index)},
                                    {QStringLiteral("today"), index == fractions.size() - 1}});
    }
    const QVariantMap activity{
        {QStringLiteral("todayText"),
         QStringLiteral("Today: 668 requests · 8 sessions · ") + compactCount(tokens.value(today))},
        {QStringLiteral("categoriesText"), QStringLiteral("Input ") + compactCount(12'000) +
                                               QStringLiteral(" · Output ") + compactCount(812'000) +
                                               QStringLiteral(" · Cache read ") + compactCount(4'200'000) +
                                               QStringLiteral(" · Cache write ") + compactCount(300'000)},
        {QStringLiteral("week"), weekBars},
    };
    m_providers.append(QVariantMap{
        {QStringLiteral("name"), QStringLiteral("Claude Code")},
        {QStringLiteral("symbol"), QStringLiteral("draw-star")},
        {QStringLiteral("updatedText"), ageText(now.addSecs(-120), now)},
        {QStringLiteral("windows"), QVariantList{window(QStringLiteral("Current session"), 23, 7800, now),
                                                 window(QStringLiteral("Current week (all models)"), 74, 4 * 86400, now),
                                                 window(QStringLiteral("Current week (Opus)"), 93, 4 * 86400, now)}},
        {QStringLiteral("activity"), activity},
        {QStringLiteral("insights"), insights()},
    });

    const auto days = heatmapDays(today);
    QHash<QDate, qint64> shownTokens;
    for (const QDate &day : days) {
        if (tokens.contains(day)) {
            shownTokens.insert(day, tokens.value(day));
        }
    }
    const auto claudeLevels = quartileLevels(shownTokens);
    const auto contributions = mockContributions(today);
    QVariantList cells;
    for (const QDate &day : days) {
        QVariantList stripes;
        const int claudeLevel = claudeLevels.value(day);
        if (claudeLevel > 0) {
            stripes.append(QVariantMap{{QStringLiteral("hue"), QStringLiteral("claude")},
                                       {QStringLiteral("level"), claudeLevel}});
        }
        const int count = contributions.value(day);
        if (count > 0) {
            stripes.append(QVariantMap{{QStringLiteral("hue"), QStringLiteral("github")},
                                       {QStringLiteral("level"), std::min(count / 2, 4)}});
        }
        const QString claudeText =
            shownTokens.contains(day) ? compactCount(shownTokens.value(day)) + QStringLiteral(" tokens")
                                      : QStringLiteral("no data");
        const QString githubText = QString::number(count) + QStringLiteral(" contributions");
        cells.append(QVariantMap{
            {QStringLiteral("line"), day.toString(Qt::ISODate) + QStringLiteral(" · Claude Code: ") + claudeText +
                                         QStringLiteral(" · GitHub: ") + githubText},
            {QStringLiteral("stripes"), stripes},
        });
    }
    m_heatmap = {
        {QStringLiteral("layers"),
         QVariantList{QVariantMap{{QStringLiteral("name"), QStringLiteral("Claude Code")},
                                  {QStringLiteral("hue"), QStringLiteral("claude")}},
                      QVariantMap{{QStringLiteral("name"), QStringLiteral("GitHub")},
                                  {QStringLiteral("hue"), QStringLiteral("github")}}}},
        {QStringLiteral("columns"), int((days.size() + 6) / 7)},
        {QStringLiteral("cells"), cells},
    };

    m_github = {
        {QStringLiteral("login"), QStringLiteral("@octocat")},
        {QStringLiteral("statsText"), QStringLiteral("Today 5 · Streak 12 days · Year ") + QLocale().toString(1234)},
        {QStringLiteral("updatedText"), ageText(now.addSecs(-120), now)},
    };
    m_label = {
        {QStringLiteral("text"), percentText(23, false, now.addSecs(7800), now)},
        {QStringLiteral("fraction"), barFraction(23)},
        {QStringLiteral("level"), levelName(meterLevel(23))},
    };
}
```

`src/applet/CMakeLists.txt` becomes:
```cmake
set_source_files_properties(Theme.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)

plasma_add_applet(org.contribusage.widget
    QML_SOURCES main.qml Theme.qml
    CPP_SOURCES appinfo.cpp appinfo.h popupmodel.cpp popupmodel.h
    GENERATE_APPLET_CLASS)
target_link_libraries(org.contribusage.widget PRIVATE contribusage_core Qt6::Qml Qt6::Quick)
```

- [ ] **Step 4: Run the tests and confirm they pass**

Run: `cmake -B build -G Ninja && cmake --build build && QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure`
Expected: all tests pass.

- [ ] **Step 5: Format and commit**

Run: `clang-format -i src/applet/*.cpp src/applet/*.h tests/applet/*.cpp && clang-format --dry-run --Werror $(git ls-files -co --exclude-standard '*.cpp' '*.h') && cmake --build build && QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure`
Expected: no format output; all tests pass.

```bash
git add src/applet tests
git commit -m "Applet: popup model with the macOS preview data, and the theme

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 4: The popup's blocks

**Files:**
- Create: `src/applet/SectionHeader.qml`, `LimitBar.qml`, `ActivitySection.qml`, `InsightsSection.qml`, `ProviderGroup.qml`, `HeatmapBlock.qml`, `GitHubSection.qml`, `Footer.qml`, `FullPopup.qml`
- Modify: `src/applet/main.qml`, `src/applet/CMakeLists.txt`, `tests/applet/tst_appletqml.cpp`

**Interfaces:**
- Consumes: `PopupModel` and `Theme` (Task 3).
- Produces: QML types `FullPopup` (the whole popup, `implicitWidth` = `Theme.popupWidth`) and its blocks.

- [ ] **Step 1: Write the failing test**

In `tests/applet/tst_appletqml.cpp`, replace the slot `mainQmlCompiles` with a data-driven one that compiles every QML file of the module:
```cpp
    void qmlFileCompiles_data() {
        QTest::addColumn<QString>("file");
        for (const auto *name : {"main", "Theme", "SectionHeader", "LimitBar", "ActivitySection", "InsightsSection",
                                 "ProviderGroup", "HeatmapBlock", "GitHubSection", "Footer", "FullPopup"}) {
            QTest::newRow(name) << QString::fromLatin1(name);
        }
    }
    void qmlFileCompiles() {
        QFETCH(QString, file);
        QQmlEngine engine;
        QQmlComponent component(
            &engine, QUrl(QStringLiteral("qrc:/qt/qml/plasma/applet/org/contribusage/widget/%1.qml").arg(file)));
        QVERIFY2(component.status() == QQmlComponent::Ready, qPrintable(component.errorString()));
    }
```

- [ ] **Step 2: Run it and confirm it fails**

Run: `cmake --build build && QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure -R tst_appletqml`
Expected: FAIL for every new row (`SectionHeader` … `FullPopup`) with `No such file or directory`; `main` and `Theme` pass.

- [ ] **Step 3: Write the blocks**

`src/applet/SectionHeader.qml`:
```qml
import QtQuick
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kde.plasma.components as PlasmaComponents

// Symbol, title, an optional subtitle and the age of the section's data (SPEC §11.2).
RowLayout {
    property string title
    property string symbol
    property string subtitle
    property string updatedText

    spacing: Theme.spacing

    Kirigami.Icon {
        source: parent.symbol
        Layout.preferredWidth: Kirigami.Units.iconSizes.small
        Layout.preferredHeight: Kirigami.Units.iconSizes.small
    }
    PlasmaComponents.Label {
        text: parent.title
        font.weight: Font.DemiBold
        Accessible.role: Accessible.Heading
    }
    PlasmaComponents.Label {
        visible: parent.subtitle.length > 0
        text: parent.subtitle
        opacity: Theme.secondaryOpacity
    }
    Item { Layout.fillWidth: true }
    PlasmaComponents.Label {
        text: parent.updatedText
        font: Kirigami.Theme.smallFont
        opacity: Theme.secondaryOpacity
    }
}
```

`src/applet/LimitBar.qml`:
```qml
import QtQuick
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kde.plasma.components as PlasmaComponents

// One usage window: label and percentage, the meter, then the reset text (SPEC §11.2, §11.4).
ColumnLayout {
    property var limit

    spacing: 2
    Accessible.name: limit.label + ", " + limit.percentText + " used, " + limit.resetText

    RowLayout {
        PlasmaComponents.Label { text: limit.label; Layout.fillWidth: true }
        PlasmaComponents.Label { text: limit.percentText; font.features: ({ "tnum": 1 }) }
    }
    Rectangle {
        Layout.fillWidth: true
        implicitHeight: Theme.meterHeight
        radius: height / 2
        color: Qt.alpha(Kirigami.Theme.textColor, Theme.trackOpacity)

        Rectangle {
            width: parent.width * limit.fraction
            height: parent.height
            radius: parent.radius
            color: Theme.meterColor(limit.level)
        }
    }
    PlasmaComponents.Label {
        visible: limit.resetText.length > 0
        text: limit.resetText
        font: Kirigami.Theme.smallFont
        opacity: Theme.secondaryOpacity
        Layout.alignment: Qt.AlignRight
    }
}
```

`src/applet/ActivitySection.qml`:
```qml
import QtQuick
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kde.plasma.components as PlasmaComponents

// US-5: today's numbers, the token categories, then total tokens of the last 7 days, today in full (SPEC §11.2).
ColumnLayout {
    property var activity

    spacing: 4

    PlasmaComponents.Label { text: "On this machine"; opacity: Theme.secondaryOpacity }
    PlasmaComponents.Label { text: activity.todayText }
    PlasmaComponents.Label {
        text: activity.categoriesText
        font: Kirigami.Theme.smallFont
        opacity: Theme.secondaryOpacity
        wrapMode: Text.Wrap
        Layout.fillWidth: true
    }
    RowLayout {
        Layout.fillWidth: true

        Row {
            id: bars
            spacing: Theme.spacing
            Layout.fillWidth: true
            Layout.preferredHeight: Theme.weekChartHeight
            Accessible.name: "Tokens per day, last 7 days"

            Repeater {
                model: activity.week
                Rectangle {
                    required property var modelData
                    width: (bars.width - bars.spacing * 6) / 7
                    height: Math.max(2, Theme.weekChartHeight * modelData.fraction)
                    anchors.bottom: parent.bottom
                    radius: 2
                    color: Theme.accent
                    opacity: modelData.today ? 1 : Theme.mutedBarOpacity
                }
            }
        }
        PlasmaComponents.Label {
            text: "last 7 days"
            font: Kirigami.Theme.smallFont
            opacity: Theme.secondaryOpacity
            Layout.alignment: Qt.AlignBottom
        }
    }
}
```

`src/applet/InsightsSection.qml`:
```qml
import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kde.plasma.components as PlasmaComponents

// FR-38: collapsed by default; one period at a time, its shares, then the top three of each ranking.
ColumnLayout {
    property var insights
    property bool expanded: false
    property int period: 0
    readonly property var shown: insights.periods[period]

    spacing: 4

    PlasmaComponents.ToolButton {
        text: "Insights"
        icon.name: parent.expanded ? "arrow-down" : "arrow-right"
        onClicked: parent.expanded = !parent.expanded
        QQC2.ToolTip.text: insights.note
        QQC2.ToolTip.visible: hovered
        QQC2.ToolTip.delay: 200
    }
    ColumnLayout {
        visible: parent.expanded
        spacing: 3
        Layout.fillWidth: true

        RowLayout {
            Repeater {
                model: insights.periods
                PlasmaComponents.TabButton {
                    required property var modelData
                    required property int index
                    text: modelData.label
                    checked: index === period
                    onClicked: period = index
                }
            }
        }
        PlasmaComponents.Label { text: shown.summary; font: Kirigami.Theme.smallFont; opacity: Theme.secondaryOpacity }
        Repeater {
            model: shown.rows
            delegate: shareRow
        }
        Repeater {
            model: shown.rankings
            ColumnLayout {
                required property var modelData
                spacing: 3
                Layout.fillWidth: true
                PlasmaComponents.Label {
                    text: modelData.title
                    font: Kirigami.Theme.smallFont
                    opacity: Theme.secondaryOpacity
                    Layout.topMargin: 5
                }
                Repeater {
                    model: modelData.rows
                    delegate: shareRow
                }
            }
        }
    }

    Component {
        id: shareRow
        RowLayout {
            required property var modelData
            Layout.fillWidth: true
            PlasmaComponents.Label { text: modelData.label; elide: Text.ElideMiddle; Layout.fillWidth: true }
            PlasmaComponents.Label { text: modelData.percent; opacity: Theme.secondaryOpacity }
        }
    }
}
```

`src/applet/ProviderGroup.qml`:
```qml
import QtQuick
import QtQuick.Layouts

// One provider: header, limits, activity, insights (FR-4, SPEC §11.2).
ColumnLayout {
    property var provider

    spacing: Theme.spacing

    SectionHeader {
        title: provider.name
        symbol: provider.symbol
        updatedText: provider.updatedText
        Layout.fillWidth: true
    }
    Repeater {
        model: provider.windows
        LimitBar {
            required property var modelData
            limit: modelData
            Layout.fillWidth: true
        }
    }
    ActivitySection {
        activity: provider.activity
        Layout.fillWidth: true
    }
    InsightsSection {
        insights: provider.insights
        Layout.fillWidth: true
    }
}
```

`src/applet/HeatmapBlock.qml`:
```qml
import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kde.plasma.components as PlasmaComponents

// The shared heatmap, Combined style (FR-48, FR-49, ADR-032): a legend, then one grid of week columns from Sunday;
// a cell holds a stripe per layer active that day, or the neutral fill.
ColumnLayout {
    property var heatmap
    property string hoveredLine: ""

    spacing: 6

    RowLayout {
        Layout.fillWidth: true
        SectionHeader { title: "Heatmap"; symbol: "view-calendar"; Layout.fillWidth: true }
        Repeater {
            model: heatmap.layers
            RowLayout {
                required property var modelData
                spacing: 4
                Rectangle { width: 8; height: 8; radius: 2; color: Theme.hue(modelData.hue) }
                PlasmaComponents.Label {
                    text: modelData.name
                    font: Kirigami.Theme.smallFont
                    opacity: Theme.secondaryOpacity
                }
            }
        }
    }

    Grid {
        id: grid
        readonly property real side: (width - Theme.cellGap * (heatmap.columns - 1)) / heatmap.columns

        Layout.fillWidth: true
        Layout.preferredHeight: 7 * side + 6 * Theme.cellGap
        rows: 7
        flow: Grid.TopToBottom
        spacing: Theme.cellGap

        Repeater {
            model: heatmap.cells
            Rectangle {
                id: cell
                required property var modelData
                width: grid.side
                height: grid.side
                radius: 2
                clip: true
                color: modelData.stripes.length === 0 ? Qt.alpha(Kirigami.Theme.textColor, Theme.emptyCellOpacity)
                                                      : "transparent"
                Accessible.name: modelData.line

                Row {
                    anchors.fill: parent
                    Repeater {
                        model: modelData.stripes
                        Rectangle {
                            required property var modelData
                            width: grid.side / cell.modelData.stripes.length
                            height: grid.side
                            color: Theme.hue(modelData.hue)
                            opacity: Theme.levelOpacity(modelData.level)
                        }
                    }
                }
                HoverHandler {
                    id: hoverHandler
                    onHoveredChanged: if (hovered) hoveredLine = cell.modelData.line
                }
                QQC2.ToolTip.text: modelData.line
                QQC2.ToolTip.visible: hoverHandler.hovered
                QQC2.ToolTip.delay: 200
            }
        }
    }

    PlasmaComponents.Label {
        text: hoveredLine.length > 0 ? hoveredLine : heatmap.cells[heatmap.cells.length - 1].line
        font: Kirigami.Theme.smallFont
        opacity: Theme.secondaryOpacity
        elide: Text.ElideRight
        Layout.fillWidth: true
    }
}
```

`src/applet/GitHubSection.qml`:
```qml
import QtQuick
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kde.plasma.components as PlasmaComponents

// GitHub's header with the login, then today, the streak and the year (SPEC §11.2).
ColumnLayout {
    property var github

    spacing: 6

    SectionHeader {
        title: "GitHub"
        symbol: "vcs-commit"
        subtitle: github.login
        updatedText: github.updatedText
        Layout.fillWidth: true
    }
    PlasmaComponents.Label { text: github.statsText; font: Kirigami.Theme.smallFont }
}
```

`src/applet/Footer.qml`:
```qml
import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.plasma.components as PlasmaComponents
import org.kde.plasma.plasmoid

// Refresh, Settings and Copy as icons with tooltips (SPEC §11.2). A widget has no Quit; Plasma removes it.
RowLayout {
    PlasmaComponents.ToolButton {
        icon.name: "view-refresh"
        display: QQC2.AbstractButton.IconOnly
        text: "Refresh"
        QQC2.ToolTip.text: text
        QQC2.ToolTip.visible: hovered
        // Wired to the refresh coordinator in L-5.
        enabled: false
    }
    Item { Layout.fillWidth: true }
    PlasmaComponents.ToolButton {
        icon.name: "configure"
        display: QQC2.AbstractButton.IconOnly
        text: "Settings"
        QQC2.ToolTip.text: text
        QQC2.ToolTip.visible: hovered
        onClicked: Plasmoid.internalAction("configure").trigger()
    }
    Item { Layout.fillWidth: true }
    PlasmaComponents.ToolButton {
        icon.name: "edit-copy"
        display: QQC2.AbstractButton.IconOnly
        text: "Copy statistics"
        QQC2.ToolTip.text: text
        QQC2.ToolTip.visible: hovered
        // Copy (FR-42) arrives with the real data.
        enabled: false
    }
}
```

`src/applet/FullPopup.qml`:
```qml
import QtQuick
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

// SPEC §11.2: the blocks in the default order, separated by dividers, then the footer.
ColumnLayout {
    implicitWidth: Theme.popupWidth
    Layout.preferredWidth: Theme.popupWidth
    spacing: 0

    Repeater {
        model: PopupModel.providers
        ProviderGroup {
            required property var modelData
            provider: modelData
            Layout.fillWidth: true
            Layout.margins: Theme.padding
        }
    }
    Rectangle { Layout.fillWidth: true; implicitHeight: 1; color: Qt.alpha(Kirigami.Theme.textColor, Theme.dividerOpacity) }
    HeatmapBlock {
        heatmap: PopupModel.heatmap
        Layout.fillWidth: true
        Layout.margins: Theme.padding
    }
    Rectangle { Layout.fillWidth: true; implicitHeight: 1; color: Qt.alpha(Kirigami.Theme.textColor, Theme.dividerOpacity) }
    GitHubSection {
        github: PopupModel.github
        Layout.fillWidth: true
        Layout.margins: Theme.padding
    }
    Rectangle { Layout.fillWidth: true; implicitHeight: 1; color: Qt.alpha(Kirigami.Theme.textColor, Theme.dividerOpacity) }
    Footer {
        Layout.fillWidth: true
        Layout.margins: Theme.spacing
    }
}
```

`src/applet/main.qml` (the full representation and the size at which a desktop or the viewer shows the popup inline):
```qml
import QtQuick
import org.kde.kirigami as Kirigami
import org.kde.plasma.components as PlasmaComponents
import org.kde.plasma.plasmoid

PlasmoidItem {
    id: root

    switchWidth: Theme.popupWidth
    switchHeight: Kirigami.Units.gridUnit * 20

    compactRepresentation: PlasmaComponents.Label {
        text: "contribusage"

        MouseArea {
            anchors.fill: parent
            onClicked: root.expanded = !root.expanded
        }
    }

    fullRepresentation: FullPopup {}
}
```

In `src/applet/CMakeLists.txt`, extend `QML_SOURCES`:
```cmake
    QML_SOURCES main.qml Theme.qml SectionHeader.qml LimitBar.qml ActivitySection.qml InsightsSection.qml
        ProviderGroup.qml HeatmapBlock.qml GitHubSection.qml Footer.qml FullPopup.qml
```

- [ ] **Step 4: Run the tests and confirm they pass**

Run: `cmake -B build -G Ninja && cmake --build build && QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure`
Expected: all tests pass, and every `qmlFileCompiles` row is PASS.

- [ ] **Step 5: Look at it next to the macOS screenshot**

Run: `QT_PLUGIN_PATH=$PWD/build/bin plasmoidviewer -a org.contribusage.widget -f planar -s 420x900`, then capture the window with `spectacle -b -n -a -o <scratchpad>/popup.png` after 4 seconds.
Expected: the popup appears inline, in this order: the Claude Code header with "updated 2 min ago"; three bars (blue 23 %, orange 74 %, red 93 %) with their reset texts; "On this machine", the today line, the categories line and 7 bars with the last at full strength; "Insights"; a divider; the Heatmap header with the legend and 26 week columns of orange, green and split cells; the GitHub header with "@octocat" and its stats line; and the footer icons. The viewer's terminal prints no QML warnings. Compare it with `docs/assets/screenshot-dark.png` and note visible differences in the ledger (the font differs on purpose, see the port design §3).

- [ ] **Step 6: Commit**

```bash
git add src/applet tests
git commit -m "Applet: the popup's blocks with the macOS look

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 5: The panel label in the `prompt` style

**Files:**
- Create: `src/applet/PromptGlyph.qml`, `src/applet/CompactLabel.qml`
- Modify: `src/applet/main.qml`, `src/applet/CMakeLists.txt`, `tests/applet/tst_appletqml.cpp`

**Interfaces:**
- Consumes: `PopupModel.label`, `Theme` (Task 3).
- Produces: QML type `PromptGlyph { fraction: real; fillColor: color }`, drawn in an 18-unit design box scaled to its height (ADR-035's mark); and `CompactLabel`.

- [ ] **Step 1: Write the failing test**

In `tst_appletqml.cpp`, add `"PromptGlyph", "CompactLabel"` to the names list of `qmlFileCompiles_data`.

- [ ] **Step 2: Run it and confirm it fails**

Run: `cmake --build build && QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure -R tst_appletqml`
Expected: FAIL for the rows `PromptGlyph` and `CompactLabel`.

- [ ] **Step 3: Write the label**

`src/applet/PromptGlyph.qml`:
```qml
import QtQuick
import QtQuick.Shapes
import org.kde.kirigami as Kirigami

// ADR-035's mark: a prompt, then three code lines the agent "types" as the window fills. Drawn in an 18 × 16 design box
// (MenuBarArt.swift's units) and scaled to the item's height.
Item {
    id: glyph

    property real fraction: 0
    property color fillColor: Theme.accent
    readonly property real unit: height / 18
    // Code lines as (x, length) in design units, top to bottom; their y is 3.5, 9 and 14.5.
    readonly property var lines: [[6.7, 9.0], [3.7, 12.0], [3.7, 7.5]]

    implicitHeight: Kirigami.Units.iconSizes.small
    implicitWidth: 16 * unit

    Shape {
        anchors.fill: parent
        preferredRendererType: Shape.CurveRenderer

        ShapePath {
            strokeColor: Kirigami.Theme.textColor
            strokeWidth: 1.8 * glyph.unit
            fillColor: "transparent"
            capStyle: ShapePath.RoundCap
            joinStyle: ShapePath.RoundJoin
            startX: 1 * glyph.unit
            startY: (3.5 - 2.8) * glyph.unit
            PathLine { x: (1 + 2.94) * glyph.unit; y: 3.5 * glyph.unit }
            PathLine { x: 1 * glyph.unit; y: (3.5 + 2.8) * glyph.unit }
        }
    }

    Repeater {
        model: 3
        Item {
            required property int index
            readonly property real lineX: glyph.lines[index][0]
            readonly property real length: glyph.lines[index][1]
            // Fill bar after bar, up to `fraction` of their total length.
            readonly property real before: index === 0 ? 0 : index === 1 ? 9.0 : 21.0
            readonly property real filled: Math.max(0, Math.min(length, glyph.fraction * 28.5 - before))

            x: lineX * glyph.unit
            y: (3.5 + 5.5 * index - 1.1) * glyph.unit
            width: length * glyph.unit
            height: 2.2 * glyph.unit

            Rectangle {
                anchors.fill: parent
                radius: height / 2
                color: Qt.alpha(Kirigami.Theme.textColor, 0.25)
            }
            Rectangle {
                visible: parent.filled > 0
                width: parent.filled * glyph.unit
                height: parent.height
                radius: height / 2
                color: glyph.fillColor
            }
        }
    }
}
```

`src/applet/CompactLabel.qml`:
```qml
import QtQuick
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kde.plasma.components as PlasmaComponents
import org.kde.plasma.core as PlasmaCore
import org.kde.plasma.plasmoid

// SPEC §11.1 in the `prompt` style: the mark filled with the session window, then its value; a vertical panel has
// room for the mark only.
MouseArea {
    id: label

    readonly property bool vertical: Plasmoid.formFactor === PlasmaCore.Types.Vertical
    readonly property real glyphHeight: Math.min(height, Kirigami.Units.iconSizes.smallMedium)

    signal toggled

    Layout.minimumWidth: row.implicitWidth
    Layout.preferredWidth: row.implicitWidth
    Layout.minimumHeight: vertical ? glyphHeight : 0
    Accessible.name: "Claude Code " + PopupModel.label.text
    onClicked: toggled()

    RowLayout {
        id: row
        anchors.centerIn: parent
        spacing: 4

        PromptGlyph {
            fraction: PopupModel.label.fraction
            fillColor: Theme.meterColor(PopupModel.label.level)
            Layout.preferredHeight: label.glyphHeight
            Layout.preferredWidth: implicitWidth
        }
        PlasmaComponents.Label {
            visible: !label.vertical
            text: PopupModel.label.text
            font.features: ({ "tnum": 1 })
        }
    }
}
```

In `src/applet/main.qml`, replace the whole `compactRepresentation: …` block with:
```qml
    compactRepresentation: CompactLabel {
        onToggled: root.expanded = !root.expanded
    }
```
and drop the now-unused `import org.kde.plasma.components as PlasmaComponents` line.

In `src/applet/CMakeLists.txt`, add `PromptGlyph.qml CompactLabel.qml` to `QML_SOURCES`.

- [ ] **Step 4: Run the tests, then look at it in both panel directions**

Run: `cmake --build build && QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure`
Expected: all tests pass.

Then run the viewer twice, taking a screenshot of each window after 4 seconds:
- `QT_PLUGIN_PATH=$PWD/build/bin plasmoidviewer -a org.contribusage.widget -f horizontal -s 300x60`. Expected: the prompt mark (chevron, then three lines with the top line and a little of the second in blue) followed by `23%`, matching the "Prompt" tile of `docs/assets/menu-bar-styles.png`. Clicking it opens the popup from Task 4.
- `QT_PLUGIN_PATH=$PWD/build/bin plasmoidviewer -a org.contribusage.widget -f vertical -s 60x300`. Expected: the mark only, without `23%` and without overflow (Review Focus 5).

- [ ] **Step 5: Format and commit**

```bash
git add src/applet tests
git commit -m "Applet: the panel label in the prompt style

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 6: Contract and vault

**Files:**
- Modify: `SPEC.md` (§17.8), `docs/superpowers/specs/2026-10-04-linux-plasmoid-port-design.md` (§4 order), `llm-wiki/modules/applet.md`, `llm-wiki/modules/core.md`, `llm-wiki/log.md`

- [ ] **Step 1: Record the order change and tick the task**

In `SPEC.md` §17.8, directly after the L-0 line, insert:
```markdown
- [x] **L-4a** Base look with mock data, built ahead of L-1 to L-3 so the look can be checked early: the `prompt` panel label and the popup's blocks in their loaded state, filled with the macOS app's preview data through the core's display and heatmap functions. Accept: `tst_display`, `tst_heatmap` and `tst_appletqml` pass; the viewer shows the popup and the label as in `docs/assets/screenshot-dark.png` and the Prompt tile of `docs/assets/menu-bar-styles.png`.
```
and change the L-4 line to:
```markdown
- [ ] **L-4** Widget with the base look on real data: the remaining label styles, the section states of SPEC §11.3, keyboard navigation in the heatmap.
```

In the port design, §4, under the milestone table, add:
```markdown
On 2026-10-04 the base look moved ahead of M1 to M3 as L-4a (mock data), so the look could be checked early; M4 keeps the rest of the UI on real data.
```

- [ ] **Step 2: Update the vault**

`llm-wiki/modules/applet.md`: set `updated: 2026-10-04`, set `tracks: [src/applet, tests/applet]` (unchanged), and replace the `## Contract` section's bullets with:
```markdown
- QML singletons `AppInfo` (`version`), `PopupModel` (`providers`, `heatmap`, `github`, `label`; mock data until L-1 to L-3) and `Theme` (every color, size and opacity; the customization layers of L-6 override it).
- `main.qml`: a `PlasmoidItem` with `CompactLabel` (the `prompt` style) and `FullPopup` (the blocks of SPEC §11.2: `ProviderGroup` with `LimitBar`, `ActivitySection` and `InsightsSection`, then `HeatmapBlock`, `GitHubSection` and `Footer`).
```
and add to `## Things that bite`:
```markdown
- Colors live in `Theme.qml` only; text colors come from `Kirigami.Theme` in each component, so the popup follows a light or dark Plasma theme.
- `switchWidth`/`switchHeight` make a desktop (and `plasmoidviewer -f planar`) show the popup inline, which is how its look is checked.
```

`llm-wiki/modules/core.md`: set `updated: 2026-10-04`, set `tracks: [src/core, tests/core]`, and add directly under the port note added in M0:
```markdown
> In C++ so far: `display.h` (SPEC §11.4 and §11.5: meter levels, percent, reset, age and compact count texts, bar fractions) and `heatmap.h` (FR-20 and FR-48: the 26-week range and quartile levels), with `tst_display` and `tst_heatmap`.
```

`llm-wiki/log.md`: append:
```markdown

## [2026-10-04] feat | Base look with mock data (L-4a)
- The `prompt` panel label and the popup's blocks in the macOS look, filled with the macOS preview data through new core display and heatmap functions.
- Built ahead of L-1 to L-3 so the look can be checked early; `Theme.qml` holds every color and size for the customization layers.
```

- [ ] **Step 3: Run every check and commit**

Run: `llm-wiki/tools/wiki.sh lint && cmake --build build && QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure && clang-format --dry-run --Werror $(git ls-files '*.cpp' '*.h') && echo all-ok`
Expected: `wiki lint: clean`, all tests pass, then `all-ok`.

```bash
git add SPEC.md docs/superpowers/specs llm-wiki
git commit -m "Spec: L-4a base look ahead of the logic milestones

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```
