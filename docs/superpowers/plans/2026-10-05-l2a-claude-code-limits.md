# L-2a: Claude Code Limits, Live Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Show the real Claude Code plan limits in the widget. The widget finds `claude`, runs the `/usage` probe every 5 minutes, parses the output, keeps the last result across restarts, and shows the limit bars, the insights and an honest status line. Blocks without a data source yet (activity, heatmap, GitHub) are hidden in live mode instead of showing mock numbers.

**Architecture:** A new static library, `contribusage_claudecode` (`src/claudecode/`), links `contribusage_core`; the core never includes it (AGENTS.md). It holds:
- the parser
- the locator
- the probe with its error mapping
- a small `LimitsJob` QObject: the interval timer, the cache and the source state
- `limitsView`: the state mapped to what QML reads

The applet's `PopupModel` owns these in live mode. The macOS preview data stays as a fake mode (`CONTRIBUSAGE_FAKE_DATA=1`, as ADR-036's debug flag), which the applet tests use. Everything runs on Qt's event loop through the L-1a process runner, so nothing blocks plasmashell.

**Tech Stack:** C++23, Qt 6 Core (`QRegularExpression`, `QTimeZone`, `QProcessEnvironment`, `QTimer`), QML, Qt Test.

**Spec:** SPEC §8.1 (invocation, output, parsing rules P-1 to P-11, classification), FR-6 (locate), FR-7 (probe), FR-9 (unparseable), FR-10 (persist), FR-11 (reset), §11.3 (section states), §13 (error texts), ADR-017 (unsupported plan), ADR-039 (5 min default), ADR-042 (one week bar). The port design is `docs/superpowers/specs/2026-10-04-linux-plasmoid-port-design.md` §1 and §2. The Swift reference, read with `git show 2603a21:<path>`:
- `Packages/ContribusageKit/Sources/ContribusageClaudeCode/Limits/UsageParser.swift`
- `Packages/ContribusageKit/Sources/ContribusageClaudeCode/Limits/ClaudeLocator.swift`
- `Packages/ContribusageKit/Sources/ContribusageClaudeCode/ClaudeCodeProvider.swift`
- `App/Popover/SectionStateView.swift` (error texts)
- their tests in `Packages/ContribusageKit/Tests/ContribusageClaudeCodeTests/`, with the fixtures in `Fixtures/usage/`

**Found while planning:** `claude` 2.1.289 prints reset times as `Oct 5, 6am (Europe/Berlin)`, a format P-7 does not list. The parser adds `MMM d, h:mm a` and `MMM d, h a`, with a scrubbed fixture of that output.

## Global Constraints

- Everything from M0, L-4a and L-1a still holds: C++23, `-Werror`, clang-format clean, `QStringLiteral`, no exceptions, and `src/core/` links only Qt6::Core and includes nothing from `src/claudecode/`.
- **Hard rule (SPEC §2, AGENTS.md):** `claude` runs only as `claude --version` (FR-6) and `claude -p /usage --no-session-persistence` (SPEC §8.1.1), plus the login shell lookup `$SHELL -lc 'command -v claude; printf …'`. The process runs in the empty probe folder `$XDG_DATA_HOME/contribusage/providers/claude-code/probe/`, with stdin `/dev/null` and a 30 s timeout (`--version` and the shell lookup: 10 s). No credential file is read, and no vendor backend is called.
- FR-6 candidates on Linux, in order: the settings override; the login shell's `command -v claude`; `~/.local/bin/claude`; `~/.npm-global/bin/claude`; `/usr/local/bin/claude`; `/usr/bin/claude`.
- Probe interval 5 min (ADR-039). A snapshot is stale after 30 min (SPEC §8.1.4). Texts are English (M0 ruling).
- Fixtures contain no personal data (SPEC §16.2): placeholder percentages and `/skill-a`-style names.
- Tests never run the real `claude`: they use `FakeProcessRunner`, or the fake data mode in the applet tests.

## Review Focus

1. **The reset format changes again** (`claude` updates often). Expected: the window stays, with `resetsAt` empty and no reset text, never a wrong date (P-9). Pinned in Task 1 (`keepsWindowWithoutResetTime`).
2. **`claude` moves after an update while the widget runs.** Expected: the next probe fails to launch, the locator runs once more, and either the new path is used or "Claude Code not found" shows. Pinned in Task 3 (`relocatesOnceWhenThePathStopsWorking`).
3. **Not logged in, API key billing, a crash or a hang.** Expected: a status line (§13 text) with the previous bars dimmed, or the unsupported plan text, never an empty or frozen widget. Pinned in Task 3 (`mapsFailures`) and Task 5 (`statusTexts`).
4. **plasmashell restarts.** Expected: the last bars show immediately, marked stale if old, then the fresh probe replaces them (FR-10). Pinned in Task 4 (`roundTrips`) and Task 5 (`startsWithTheCachedSnapshot`).
5. **The widget is removed while a probe runs.** Expected: no callback into freed objects (the L-1a context guard). Pinned in Task 5 (`aDeletedJobGetsNoLateResult`).

---

## File Structure

| Path | Responsibility |
|---|---|
| `src/core/display.h`, `src/core/display.cpp` | Add `meterLevelName` and `errorMessage` (SPEC §13) |
| `src/core/limitscache.h`, `src/core/limitscache.cpp` | FR-10: last limits snapshot per provider in `state.json` |
| `src/claudecode/CMakeLists.txt` | `contribusage_claudecode` static library |
| `src/claudecode/usageparser.h/.cpp` | P-1 to P-11, window classification |
| `src/claudecode/claudelocator.h/.cpp` | FR-6 |
| `src/claudecode/claudecodelimits.h/.cpp` | The probe (FR-7) and its error mapping (P-10, FR-9) |
| `src/claudecode/limitsjob.h/.cpp` | Interval, cache, source state |
| `src/claudecode/limitsview.h/.cpp` | Source state → QVariantMap for QML (windows, status, insights, label) |
| `src/applet/popupmodel.h/.cpp` | Live mode (owns the above) and fake mode |
| `src/applet/ProviderGroup.qml`, `FullPopup.qml` | Status line; blocks shown only when they have data |
| `tests/support/fakeprocessrunner.h` | Scripted `ProcessRunning` for tests |
| `tests/claudecode/fixtures/usage/*.txt` | `/usage` fixtures |
| `tests/claudecode/tst_*.cpp`, `tests/core/tst_limitscache.cpp` | Tests |

---

### Task 1: The parser

**Files:**
- Create: `src/claudecode/CMakeLists.txt`, `src/claudecode/usageparser.h`, `src/claudecode/usageparser.cpp`, `tests/claudecode/tst_usageparser.cpp`, `tests/claudecode/fixtures/usage/subscription-basic.txt`, `logged-out.txt`, `not-subscription.txt` (copied from `2603a21`), `tests/claudecode/fixtures/usage/subscription-comma-dates.txt` (new)
- Modify: `CMakeLists.txt` (root), `tests/CMakeLists.txt`

**Interfaces:**
- Produces (namespace `contribusage::claudecode`, header `claudecode/usageparser.h`): `QString providerId()` (`"claude-code"`), `WindowKind windowKind(const QString &label)`, `LimitsReport parseUsage(const QString &output, const QDateTime &now, const QTimeZone &fallbackZone = QTimeZone::systemTimeZone())`.
- CMake target `contribusage_claudecode` (STATIC, PIC, links `contribusage_core`).

- [ ] **Step 1: Copy the fixtures and write the new one**

```bash
mkdir -p tests/claudecode/fixtures/usage
for f in subscription-basic logged-out not-subscription; do
  git show 2603a21:Packages/ContribusageKit/Tests/ContribusageClaudeCodeTests/Fixtures/usage/$f.txt > tests/claudecode/fixtures/usage/$f.txt
done
```

`tests/claudecode/fixtures/usage/subscription-comma-dates.txt` (the format of Claude Code 2.1.289, numbers and names replaced):
```text
You are currently using your subscription to power your Claude Code usage

Current session: 12% used · resets Oct 5, 6am (Europe/Berlin)
Current week (all models): 34% used · resets Oct 7, 1:30pm (Europe/Berlin)

What's contributing to your limits usage?
Approximate, based on local sessions on this machine — does not include other devices or claude.ai. Behaviors are independent characteristics, not a breakdown.

Last 24h · 400 requests · 7 sessions
  90% of your usage came from subagent-heavy sessions
  80% of your usage was at >150k context
  Top skills: /skill-a 13%, /skill-b 4%, /plugin-a:skill-c 3%
  Top subagents: general-purpose 3%, plugin-a:agent-b 1%
  Top plugins: plugin-a 23%
```

- [ ] **Step 2: Write the failing test**

`tests/claudecode/tst_usageparser.cpp`:
```cpp
#include "claudecode/usageparser.h"

#include <QFile>
#include <QTest>
#include <QTimeZone>

using namespace contribusage;
using namespace contribusage::claudecode;

namespace {

QDateTime utc(const char *iso) {
    return QDateTime::fromString(QString::fromLatin1(iso), Qt::ISODate).toUTC();
}

QString fixture(const char *name) {
    QFile file(QStringLiteral(CONTRIBUSAGE_FIXTURES "/usage/") + QString::fromLatin1(name) + QStringLiteral(".txt"));
    return file.open(QIODevice::ReadOnly) ? QString::fromUtf8(file.readAll()) : QString();
}

QList<UsageWindow> parse(const QString &output, const QDateTime &now, const QTimeZone &zone = QTimeZone::UTC) {
    return parseUsage(output, now, zone).windows;
}

} // namespace

// SPEC §8.1.3 and §8.1.4, ported from UsageParserTests.swift.
class TestUsageParser : public QObject {
    Q_OBJECT

private Q_SLOTS:
    // Appendix A: the expected parse with now = 2026-09-27 20:00 UTC.
    void parsesSubscriptionBaseline() {
        const auto windows = parse(fixture("subscription-basic"), utc("2026-09-27T20:00:00Z"));
        QCOMPARE(windows.size(), 2);
        QVERIFY(windows.at(0) == (UsageWindow{.label = QStringLiteral("Current session"),
                                              .kind = WindowKind::Session,
                                              .usedPercent = 3,
                                              .isBelowOne = false,
                                              .resetsAt = utc("2026-09-28T02:09:00Z")}));
        QVERIFY(windows.at(1) == (UsageWindow{.label = QStringLiteral("Current week (all models)"),
                                              .kind = WindowKind::Weekly,
                                              .usedPercent = 5,
                                              .isBelowOne = false,
                                              .resetsAt = utc("2026-10-04T16:59:00Z")}));
    }

    // Claude Code 2.1.289: "Oct 5, 6am" and "Oct 7, 1:30pm".
    void parsesCommaDates() {
        const auto windows = parse(fixture("subscription-comma-dates"), utc("2026-10-05T00:00:00Z"));
        QCOMPARE(windows.size(), 2);
        QCOMPARE(windows.at(0).resetsAt, std::optional(utc("2026-10-05T04:00:00Z")));
        QCOMPARE(windows.at(1).resetsAt, std::optional(utc("2026-10-07T11:30:00Z")));
        QCOMPARE(windows.at(1).usedPercent, 34.0);
    }

    // P-1: color codes around and inside lines change nothing.
    void stripsAnsi() {
        const QString plain = fixture("subscription-basic");
        QString colored;
        for (const QString &line : plain.split(u'\n')) {
            colored += QStringLiteral("\x1b[1;32m") + QString(line).replace(u':', QStringLiteral("\x1b[0m:")) +
                       QStringLiteral("\x1b[0m\n");
        }
        const QDateTime now = utc("2026-09-27T20:00:00Z");
        QVERIFY(parse(colored, now) == parse(plain, now));
    }

    // P-5: "<1%" is 0.5 with the flag set.
    void parsesBelowOne() {
        const auto windows = parse(QStringLiteral("Current session: <1% used"), QDateTime::currentDateTimeUtc());
        QCOMPARE(windows.size(), 1);
        QCOMPARE(windows.at(0).usedPercent, 0.5);
        QVERIFY(windows.at(0).isBelowOne);
        QVERIFY(!windows.at(0).resetsAt);
    }

    // P-6 to P-9 with SPEC §16.3's DST and New Year cases; the fallback zone is UTC.
    void resolvesResetTime_data() {
        QTest::addColumn<QString>("clause");
        QTest::addColumn<QDateTime>("now");
        QTest::addColumn<QDateTime>("expected");
        const auto row = [](const char *name, const char *clause, const char *now, const char *expected) {
            QTest::newRow(name) << QString::fromUtf8(clause) << utc(now) << utc(expected);
        };
        row("date and time", "resets Sep 28 at 4:09am (Europe/Berlin)", "2026-09-27T20:00:00Z", "2026-09-28T02:09:00Z");
        row("date and hour", "resets Sep 28 at 4am (Europe/Berlin)", "2026-09-27T20:00:00Z", "2026-09-28T02:00:00Z");
        row("comma, hour", "resets Oct 5, 6am (Europe/Berlin)", "2026-10-05T00:00:00Z", "2026-10-05T04:00:00Z");
        row("comma, time", "resets Oct 7, 1:30pm (Europe/Berlin)", "2026-10-05T00:00:00Z", "2026-10-07T11:30:00Z");
        row("date only", "resets Sep 28 (Europe/Berlin)", "2026-09-27T20:00:00Z", "2026-09-27T22:00:00Z");
        row("time only", "resets 4:09am (Europe/Berlin)", "2026-09-27T20:00:00Z", "2026-09-28T02:09:00Z");
        row("hour only", "resets 11pm (Europe/Berlin)", "2026-09-27T20:00:00Z", "2026-09-27T21:00:00Z");
        row("recent past stays", "resets Sep 27 at 4am (Europe/Berlin)", "2026-09-27T20:00:00Z", "2026-09-27T02:00:00Z");
        row("older rolls over", "resets Sep 25 at 4am (Europe/Berlin)", "2026-09-27T20:00:00Z", "2027-09-25T02:00:00Z");
        row("across DST", "resets Oct 26 at 4:09am (Europe/Berlin)", "2026-10-20T12:00:00Z", "2026-10-26T03:09:00Z");
        row("time on DST day", "resets 4:09am (Europe/Berlin)", "2026-10-24T12:00:00Z", "2026-10-25T03:09:00Z");
        row("New Year", "resets Jan 2 at 6:59pm (Europe/Berlin)", "2026-12-30T12:00:00Z", "2027-01-02T17:59:00Z");
        row("unknown zone", "resets Sep 28 at 4:09am (Mars/Olympus)", "2026-09-27T20:00:00Z", "2026-09-28T04:09:00Z");
        row("no zone", "resets Sep 28 at 4:09 AM", "2026-09-27T20:00:00Z", "2026-09-28T04:09:00Z");
    }
    void resolvesResetTime() {
        QFETCH(QString, clause);
        QFETCH(QDateTime, now);
        QFETCH(QDateTime, expected);
        const auto windows = parse(QStringLiteral("Current session: 3% used · ") + clause, now);
        QCOMPARE(windows.size(), 1);
        QCOMPARE(windows.at(0).resetsAt, std::optional(expected));
    }

    // P-9: an unreadable or missing reset clause keeps the window.
    void keepsWindowWithoutResetTime_data() {
        QTest::addColumn<QString>("line");
        QTest::newRow("unreadable") << QStringLiteral("Current week (all models): 42.5% used · resets whenever");
        QTest::newRow("missing") << QStringLiteral("Current week (all models): 42.5% used");
    }
    void keepsWindowWithoutResetTime() {
        QFETCH(QString, line);
        const auto windows = parse(line, QDateTime::currentDateTimeUtc());
        QCOMPARE(windows.size(), 1);
        QVERIFY(windows.at(0) == (UsageWindow{.label = QStringLiteral("Current week (all models)"),
                                              .kind = WindowKind::Weekly,
                                              .usedPercent = 42.5,
                                              .isBelowOne = false,
                                              .resetsAt = std::nullopt}));
    }

    // SPEC §8.1.4: model weeks are weekly, unknown labels stay as other.
    void classifiesWindows() {
        QCOMPARE(windowKind(QStringLiteral("Current session")), WindowKind::Session);
        QCOMPARE(windowKind(QStringLiteral("Current week (all models)")), WindowKind::Weekly);
        QCOMPARE(windowKind(QStringLiteral("Current week (Opus)")), WindowKind::Weekly);
        QCOMPARE(windowKind(QStringLiteral("Extra usage")), WindowKind::Other);
        QCOMPARE(parse(QStringLiteral("Extra usage: 42% used"), QDateTime::currentDateTimeUtc()).size(), 1);
    }

    // P-3: lines that only look similar are not windows.
    void ignoresNonWindowLines() {
        const QString text = QStringLiteral("Top skills: /skill-a 76%, /skill-b 9%\n"
                                            "  80% of your usage was at >150k context\n"
                                            "Last 24h · 668 requests\n");
        QVERIFY(parse(text, QDateTime::currentDateTimeUtc()).isEmpty());
    }

    // P-10, P-11: the billing line, the insights by period, the untouched output.
    void buildsSubscriptionReport() {
        const QString output = QStringLiteral("\x1b[1m") + fixture("subscription-basic");
        const QDateTime now = utc("2026-09-27T20:00:00Z");
        const LimitsReport report = parseUsage(output, now, QTimeZone::UTC);
        QCOMPARE(report.provider, QStringLiteral("claude-code"));
        QCOMPARE(report.billingNote,
                 std::optional(QStringLiteral("You are currently using your subscription to power your Claude Code usage")));
        QCOMPARE(report.rawOutput, std::optional(output));
        QVERIFY(report.insights);
        const Insights &insights = *report.insights;
        QVERIFY(insights.note && insights.note->startsWith(QStringLiteral("Approximate, based on local sessions")));
        QCOMPARE(insights.periods.size(), 2);
        QCOMPARE(insights.periods.at(0).label, QStringLiteral("Last 24h"));
        QCOMPARE(insights.periods.at(1).summary, QStringLiteral("3002 requests · 63 sessions"));
        const InsightsPeriod &week = insights.periods.at(1);
        QVERIFY(week.shares == (QList<Share>{{.label = QStringLiteral("At >150k context"), .percent = 72},
                                             {.label = QStringLiteral("Sessions active for 8+ hours"), .percent = 13},
                                             {.label = QStringLiteral("Subagent-heavy sessions"), .percent = 12}}));
        QCOMPARE(week.rankings.size(), 3);
        QCOMPARE(week.rankings.at(0).title, QStringLiteral("Skills"));
        QCOMPARE(week.rankings.at(1).title, QStringLiteral("Subagents"));
        QVERIFY(week.rankings.at(0).items.at(1) == (Share{.label = QStringLiteral("/skill-d"), .percent = 18}));
        QVERIFY(week.rankings.at(2).items == (QList<Share>{{.label = QStringLiteral("plugin-a"), .percent = 29}}));
    }

    // P-11: unreadable lines keep their text without a percent; a block without a period gives no insights.
    void keepsUnknownInsightLines() {
        const QString output = QStringLiteral("What's contributing to your limits usage?\n"
                                              "Last 24h · 5 requests\n"
                                              "  Mostly one project\n"
                                              "  Top models: model-a <1%\n");
        const auto insights = parseUsage(output, QDateTime::currentDateTimeUtc()).insights;
        QVERIFY(insights);
        const InsightsPeriod &period = insights->periods.at(0);
        QVERIFY(period.shares == (QList<Share>{{.label = QStringLiteral("Mostly one project"), .percent = std::nullopt}}));
        QVERIFY(period.rankings ==
                (QList<Ranking>{{.title = QStringLiteral("Models"),
                                 .items = {{.label = QStringLiteral("model-a <1%"), .percent = std::nullopt}}}}));
        QVERIFY(!parseUsage(QStringLiteral("What's contributing to your limits usage?\nA note\n"),
                            QDateTime::currentDateTimeUtc())
                     .insights);
    }

    // R-2: API key billing and logged out print the same cost summary.
    void buildsReportWithoutSubscription_data() {
        QTest::addColumn<QString>("name");
        QTest::newRow("not subscription") << QStringLiteral("not-subscription");
        QTest::newRow("logged out") << QStringLiteral("logged-out");
    }
    void buildsReportWithoutSubscription() {
        QFETCH(QString, name);
        const LimitsReport report = parseUsage(fixture(name.toLatin1().constData()), QDateTime::currentDateTimeUtc());
        QVERIFY(report.windows.isEmpty());
        QCOMPARE(report.billingNote, std::optional(QStringLiteral("Total cost:            $0.0000")));
        QVERIFY(!report.insights);
    }
};

QTEST_GUILESS_MAIN(TestUsageParser)
#include "tst_usageparser.moc"
```

Append to `tests/CMakeLists.txt`:
```cmake

ecm_add_test(claudecode/tst_usageparser.cpp
    TEST_NAME tst_usageparser
    LINK_LIBRARIES contribusage_claudecode Qt6::Test)
target_compile_definitions(tst_usageparser PRIVATE
    CONTRIBUSAGE_FIXTURES="${CMAKE_CURRENT_SOURCE_DIR}/claudecode/fixtures")
```

- [ ] **Step 3: Run it and confirm it fails**

Run: `cmake -B build -G Ninja && cmake --build build`
Expected: FAIL. CMake reports that target `contribusage_claudecode` does not exist (or the compiler reports that `claudecode/usageparser.h` is missing).

- [ ] **Step 4: Write the implementation**

`src/claudecode/CMakeLists.txt`:
```cmake
add_library(contribusage_claudecode STATIC
    usageparser.cpp
    usageparser.h)
target_include_directories(contribusage_claudecode PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_link_libraries(contribusage_claudecode PUBLIC contribusage_core)
# Linked into the shared applet library.
set_target_properties(contribusage_claudecode PROPERTIES POSITION_INDEPENDENT_CODE ON)
```

In the root `CMakeLists.txt`, after `add_subdirectory(src/core)`, add `add_subdirectory(src/claudecode)`.

`src/claudecode/usageparser.h`:
```cpp
#pragma once

#include "core/reports.h"

#include <QDateTime>
#include <QString>
#include <QTimeZone>

namespace contribusage::claudecode {

inline QString providerId() { return QStringLiteral("claude-code"); }

// SPEC §8.1.4: an unknown label is kept as Other, never dropped.
WindowKind windowKind(const QString &label);

// SPEC §8.1.3 P-1 to P-11: the report from `claude -p /usage` output. Whether the billing note means a subscription is
// the caller's decision (P-10). `fallbackZone` applies when a reset clause names no known zone (P-6).
LimitsReport parseUsage(const QString &output, const QDateTime &now,
                        const QTimeZone &fallbackZone = QTimeZone::systemTimeZone());

} // namespace contribusage::claudecode
```

`src/claudecode/usageparser.cpp`:
```cpp
#include "claudecode/usageparser.h"

#include <QRegularExpression>
#include <QStringList>

namespace contribusage::claudecode {

namespace {

// P-1.
QString stripAnsi(const QString &text) {
    static const QRegularExpression ansi(QStringLiteral("\x1b\\[[^A-Za-z]*[A-Za-z]"));
    return QString(text).remove(ansi);
}

QString capitalized(const QString &text) {
    return text.isEmpty() ? text : text.left(1).toUpper() + text.mid(1);
}

// "4:09am", "6 pm" → "4:09 AM", "6 PM", the form Qt's "AP" reads.
QString normalizedMeridiem(const QString &text) {
    static const QRegularExpression meridiem(QStringLiteral("\\s*([ap])m\\b"),
                                             QRegularExpression::CaseInsensitiveOption);
    QString result;
    qsizetype last = 0;
    auto matches = meridiem.globalMatch(text);
    while (matches.hasNext()) {
        const auto match = matches.next();
        result += text.mid(last, match.capturedStart() - last) + u' ' + match.captured(1).toUpper() + u'M';
        last = match.capturedEnd();
    }
    return result + text.mid(last);
}

// P-6 to P-9: nullopt when no format fits.
std::optional<QDateTime> resetDate(const QString &clause, const QDateTime &now, const QTimeZone &fallbackZone) {
    QString text = clause.trimmed();
    QTimeZone zone = fallbackZone;
    static const QRegularExpression zoned(QStringLiteral("^(.*?)\\s*\\(([^()]*)\\)$"));
    if (const auto match = zoned.match(text); match.hasMatch()) {
        text = match.captured(1);
        const QTimeZone named(match.captured(2).toUtf8());
        if (named.isValid()) {
            zone = named;
        }
    }
    text = normalizedMeridiem(text);

    // P-7 in order, with the comma forms of Claude Code 2.1.289 ("Oct 5, 6am").
    struct Format {
        const char *pattern;
        bool dated;
    };
    static constexpr Format formats[] = {
        {"MMM d 'at' h:mm AP", true}, {"MMM d 'at' h AP", true}, {"MMM d, h:mm AP", true}, {"MMM d, h AP", true},
        {"MMM d", true},              {"h:mm AP", false},        {"h AP", false},
    };
    const QDateTime local = now.toTimeZone(zone);
    for (const Format &format : formats) {
        const QString pattern = QString::fromLatin1(format.pattern);
        if (!format.dated) {
            // P-8: a time only resolves to its next occurrence after now.
            const QTime time = QTime::fromString(text, pattern);
            if (!time.isValid()) {
                continue;
            }
            const QDateTime today(local.date(), time, zone);
            return today > now ? today : QDateTime(local.date().addDays(1), time, zone);
        }
        // P-8: this year, or next year when that lies more than 24 h in the past.
        const int year = local.date().year();
        for (const int candidate : {year, year + 1}) {
            const QDateTime parsed = QDateTime::fromString(text + u' ' + QString::number(candidate),
                                                           pattern + QStringLiteral(" yyyy"));
            if (!parsed.isValid()) {
                continue;
            }
            const QDateTime result(parsed.date(), parsed.time(), zone);
            if (candidate != year || result >= now.addSecs(-24 * 3600)) {
                return result;
            }
        }
    }
    return std::nullopt;
}

// P-3 to P-5: nullopt for any other line.
std::optional<UsageWindow> window(const QString &line, const QDateTime &now, const QTimeZone &fallbackZone) {
    static const QRegularExpression pattern(
        QStringLiteral("^\\s*(?<label>[^:]+):\\s*(?<lt><)?\\s*(?<pct>\\d+(?:\\.\\d+)?)%\\s*used"
                       "(?:.*?\\bresets\\s+(?<reset>.+?))?\\s*$"));
    const auto match = pattern.match(line);
    if (!match.hasMatch()) {
        return std::nullopt;
    }
    const QString label = match.captured(u"label").trimmed();
    const bool isBelowOne = match.hasCaptured(u"lt");
    const QString reset = match.captured(u"reset");
    return UsageWindow{.label = label,
                       .kind = windowKind(label),
                       .usedPercent = isBelowOne ? 0.5 : match.captured(u"pct").toDouble(),
                       .isBelowOne = isBelowOne,
                       .resetsAt = reset.isEmpty() ? std::nullopt : resetDate(reset, now, fallbackZone)};
}

// "72% of your usage was at >150k context", "/skill-a 29%"; anything else keeps its text and no percent.
Share share(const QString &text) {
    static const QRegularExpression usage(
        QStringLiteral("^(?<pct>\\d+)% of your usage (?:was |came from )?(?<what>.+)$"));
    static const QRegularExpression item(QStringLiteral("^(?<name>.+) (?<pct>\\d+)%$"));
    if (const auto match = usage.match(text); match.hasMatch()) {
        return {.label = capitalized(match.captured(u"what")), .percent = match.captured(u"pct").toInt()};
    }
    if (const auto match = item.match(text); match.hasMatch()) {
        return {.label = match.captured(u"name"), .percent = match.captured(u"pct").toInt()};
    }
    return {.label = text, .percent = std::nullopt};
}

InsightsPeriod period(const QString &label, const QString &summary, const QStringList &lines) {
    static const QRegularExpression top(QStringLiteral("^Top (?<title>[^:]+):\\s*(?<items>.*)$"));
    InsightsPeriod result{.label = label, .summary = summary, .shares = {}, .rankings = {}};
    for (const QString &line : lines) {
        if (const auto match = top.match(line); match.hasMatch()) {
            Ranking ranking{.title = capitalized(match.captured(u"title")), .items = {}};
            for (const QString &entry : match.captured(u"items").split(QStringLiteral(", "), Qt::SkipEmptyParts)) {
                ranking.items.append(share(entry));
            }
            result.rankings.append(ranking);
        } else {
            result.shares.append(share(line));
        }
    }
    return result;
}

// P-11: after the heading, an unindented line with " · " opens a period, indented lines fill it, any other line joins
// the note. No period, no insights.
std::optional<Insights> insights(const QStringList &block) {
    static const QRegularExpression header(QStringLiteral("^(?<label>.+?) · (?<summary>.+)$"));
    QStringList note;
    struct Open {
        QString label;
        QString summary;
        QStringList lines;
    };
    QList<Open> periods;
    for (const QString &line : block.mid(1)) {
        const QString text = line.trimmed();
        if (text.isEmpty()) {
            continue;
        }
        if (line.front().isSpace() && !periods.isEmpty()) {
            periods.last().lines.append(text);
        } else if (const auto match = header.match(text); match.hasMatch()) {
            periods.append({.label = match.captured(u"label"), .summary = match.captured(u"summary"), .lines = {}});
        } else {
            note.append(text);
        }
    }
    if (periods.isEmpty()) {
        return std::nullopt;
    }
    Insights result{.note = note.isEmpty() ? std::nullopt : std::optional(note.join(u' ')), .periods = {}};
    for (const Open &open : periods) {
        result.periods.append(period(open.label, open.summary, open.lines));
    }
    return result;
}

} // namespace

WindowKind windowKind(const QString &label) {
    if (label == u"Current session") {
        return WindowKind::Session;
    }
    return label.startsWith(QStringLiteral("Current week")) ? WindowKind::Weekly : WindowKind::Other;
}

LimitsReport parseUsage(const QString &output, const QDateTime &now, const QTimeZone &fallbackZone) {
    const QStringList lines = stripAnsi(output).split(u'\n');
    LimitsReport report{.provider = providerId(),
                        .windows = {},
                        .billingNote = std::nullopt,
                        .insights = std::nullopt,
                        .rawOutput = output};
    static const QRegularExpression contributing(QStringLiteral("^[ \\t]*What['’]s contributing"));
    for (qsizetype index = 0; index < lines.size(); ++index) {
        const QString &line = lines.at(index);
        if (!report.billingNote && !line.trimmed().isEmpty()) {
            report.billingNote = line.trimmed(); // P-10
        }
        if (contributing.match(line).hasMatch()) {
            report.insights = insights(lines.mid(index));
            break;
        }
        if (auto parsed = window(line, now, fallbackZone)) {
            report.windows.append(*parsed);
        }
    }
    return report;
}

} // namespace contribusage::claudecode
```

- [ ] **Step 5: Run the tests and confirm they pass**

Run: `cmake -B build -G Ninja && cmake --build build && ctest --test-dir build --output-on-failure -R tst_usageparser`
Expected: PASS. If a reset row fails, print `QDateTime::fromString(text + " 2026", pattern + " yyyy")` for that clause. Qt reads `MMM` with English names regardless of the system locale; if this Qt build does not, parse with `QLocale::c().toDateTime(…)` instead and record a ruling.

- [ ] **Step 6: Format, full suite, commit**

Run: `clang-format -i src/claudecode/*.cpp src/claudecode/*.h tests/claudecode/*.cpp && clang-format --dry-run --Werror $(git ls-files -co --exclude-standard '*.cpp' '*.h') && QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure`
Expected: no format output; all tests pass.

```bash
git add CMakeLists.txt src/claudecode tests
git commit -m "Claude Code: /usage parser with the comma reset format of 2.1.289

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 2: A fake runner, and the locator

**Files:**
- Create: `tests/support/fakeprocessrunner.h`, `src/claudecode/claudelocator.h`, `src/claudecode/claudelocator.cpp`, `tests/claudecode/tst_claudelocator.cpp`
- Modify: `src/claudecode/CMakeLists.txt`, `tests/CMakeLists.txt`

**Interfaces:**
- Consumes: `ProcessRunning`, `ProcessRequest`, `ProcessOutcome` (L-1a).
- Produces:
  - `FakeProcessRunner` (tests only, header `support/fakeprocessrunner.h`):
    - `answer(executable, firstArgument, outcome)` adds a rule; an empty `firstArgument` matches any.
    - `requests` lists everything that was run.
    - An unmatched request fails with `Io`.
    - Answers arrive on the next event loop pass.
  - `struct FoundClaude { QString executable; QString version; std::optional<QProcessEnvironment> environment; }`
  - `class ClaudeLocator`:
    - constructor `ClaudeLocator(ProcessRunning &runner, QString home, QString shell)`
    - `void locate(const QString &override, QObject *context, std::function<void(std::optional<FoundClaude>)> done)`

- [ ] **Step 1: Write the fake and the failing test**

`tests/support/fakeprocessrunner.h`:
```cpp
#pragma once

#include "core/process.h"

#include <QList>
#include <QPointer>
#include <QTimer>

// A scripted ProcessRunning for tests: the first rule whose executable and first argument match answers; every request
// is recorded. An unmatched request fails with Io, like a missing executable. Answers arrive on the next event loop
// pass, as the live runner's do.
class FakeProcessRunner final : public contribusage::ProcessRunning {
public:
    using ProcessRunning::run;

    struct Rule {
        QString executable;
        QString firstArgument; // empty matches any
        contribusage::ProcessOutcome outcome;
    };

    void answer(const QString &executable, const QString &firstArgument, contribusage::ProcessOutcome outcome) {
        rules.append({.executable = executable, .firstArgument = firstArgument, .outcome = std::move(outcome)});
    }

    static contribusage::ProcessOutcome exited(int code, const QString &stdoutText, const QString &stderrText = {}) {
        return contribusage::ProcessResult{.exitCode = code, .stdoutText = stdoutText, .stderrText = stderrText};
    }

    void run(const contribusage::ProcessRequest &request, QObject *context,
             contribusage::ProcessCallback done) override {
        requests.append(request);
        contribusage::ProcessOutcome outcome = std::unexpected(
            contribusage::SourceError{.kind = contribusage::SourceError::Kind::Io, .text = QStringLiteral("no rule")});
        for (const Rule &rule : rules) {
            if (rule.executable == request.executable &&
                (rule.firstArgument.isEmpty() || request.arguments.value(0) == rule.firstArgument)) {
                outcome = rule.outcome;
                break;
            }
        }
        const QPointer<QObject> guard = context;
        const bool hasContext = context != nullptr;
        QTimer::singleShot(0, [guard, hasContext, outcome, done = std::move(done)]() {
            if (!hasContext || guard) {
                done(outcome);
            }
        });
    }

    QList<Rule> rules;
    QList<contribusage::ProcessRequest> requests;
};
```

`tests/claudecode/tst_claudelocator.cpp`:
```cpp
#include "claudecode/claudelocator.h"
#include "support/fakeprocessrunner.h"

#include <QEventLoop>
#include <QTest>
#include <QTimer>

using namespace contribusage;
using namespace contribusage::claudecode;

namespace {

const QString home = QStringLiteral("/home/octocat");
const QString shell = QStringLiteral("/usr/bin/zsh");
const QString shellPath = QStringLiteral("/home/octocat/.local/bin:/usr/bin");

std::optional<FoundClaude> locate(FakeProcessRunner &runner, const QString &override = {}) {
    std::optional<std::optional<FoundClaude>> result;
    QEventLoop loop;
    ClaudeLocator locator(runner, home, shell);
    locator.locate(override, nullptr, [&](std::optional<FoundClaude> found) {
        result = std::move(found);
        loop.quit();
    });
    if (!result) {
        QTimer::singleShot(5'000, &loop, &QEventLoop::quit);
        loop.exec();
    }
    return result.value_or(std::nullopt);
}

ProcessOutcome version(const char *text) {
    return FakeProcessRunner::exited(0, QString::fromLatin1(text) + u'\n');
}

} // namespace

// FR-6, ported from ClaudeLocatorTests.swift with Linux locations.
class TestClaudeLocator : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void overrideWinsWhenValid() {
        FakeProcessRunner runner;
        runner.answer(shell, {}, FakeProcessRunner::exited(0, QStringLiteral("/usr/bin/claude\n") + shellPath + u'\n'));
        runner.answer(QStringLiteral("/custom/claude"), QStringLiteral("--version"), version("2.1.289 (Claude Code)"));
        const auto found = locate(runner, QStringLiteral("/custom/claude"));
        QVERIFY(found);
        QCOMPARE(found->executable, QStringLiteral("/custom/claude"));
        QCOMPARE(found->version, QStringLiteral("2.1.289 (Claude Code)"));
        QCOMPARE(runner.requests.at(1).arguments, QStringList{QStringLiteral("--version")});
        QCOMPARE(runner.requests.at(1).timeoutMs, 10'000);
    }

    void usesLoginShellResultAndPath() {
        FakeProcessRunner runner;
        runner.answer(shell, {},
                      FakeProcessRunner::exited(0, QStringLiteral("/home/octocat/.local/bin/claude\n") + shellPath + u'\n'));
        runner.answer(QStringLiteral("/home/octocat/.local/bin/claude"), QStringLiteral("--version"),
                      version("2.1.289 (Claude Code)"));
        const auto found = locate(runner);
        QVERIFY(found);
        QCOMPARE(found->executable, QStringLiteral("/home/octocat/.local/bin/claude"));
        QCOMPARE(runner.requests.at(0).arguments,
                 (QStringList{QStringLiteral("-lc"), QStringLiteral("command -v claude; printf '%s\\n' \"$PATH\"")}));
        QCOMPARE(runner.requests.at(0).timeoutMs, 10'000);
        QVERIFY(found->environment);
        QCOMPARE(found->environment->value(QStringLiteral("PATH")), shellPath);
        QCOMPARE(runner.requests.at(1).environment->value(QStringLiteral("PATH")), shellPath);
    }

    // Profile scripts may print before; an alias or a miss is not a path.
    void toleratesProfileOutputAndMissingClaude() {
        FakeProcessRunner runner;
        runner.answer(shell, {},
                      FakeProcessRunner::exited(0, QStringLiteral("Welcome!\nalias claude=foo\n") + shellPath + u'\n'));
        runner.answer(QStringLiteral("/usr/local/bin/claude"), QStringLiteral("--version"), version("2.0.0 (Claude Code)"));
        const auto found = locate(runner);
        QVERIFY(found);
        QCOMPARE(found->executable, QStringLiteral("/usr/local/bin/claude"));
        QCOMPARE(found->environment->value(QStringLiteral("PATH")), shellPath);
    }

    void fallsBackToKnownLocationsInOrder() {
        FakeProcessRunner runner;
        runner.answer(shell, {}, FakeProcessRunner::exited(1, {}));
        runner.answer(QStringLiteral("/usr/bin/claude"), QStringLiteral("--version"), version("2.0.0 (Claude Code)"));
        const auto found = locate(runner, QStringLiteral("/nowhere/claude"));
        QVERIFY(found);
        QCOMPARE(found->executable, QStringLiteral("/usr/bin/claude"));
        QVERIFY(!found->environment); // a failing shell inherits the environment
        QStringList tried;
        for (const auto &request : runner.requests) {
            tried.append(request.executable);
        }
        QCOMPARE(tried, (QStringList{shell, QStringLiteral("/nowhere/claude"), QStringLiteral("/home/octocat/.local/bin/claude"),
                                     QStringLiteral("/home/octocat/.npm-global/bin/claude"),
                                     QStringLiteral("/usr/local/bin/claude"), QStringLiteral("/usr/bin/claude")}));
    }

    void skipsCandidatesWhoseVersionFails() {
        FakeProcessRunner runner;
        runner.answer(shell, {}, FakeProcessRunner::exited(1, {}));
        runner.answer(QStringLiteral("/home/octocat/.local/bin/claude"), QStringLiteral("--version"),
                      FakeProcessRunner::exited(127, {}));
        runner.answer(QStringLiteral("/usr/local/bin/claude"), QStringLiteral("--version"), version("2.0.0 (Claude Code)"));
        const auto found = locate(runner);
        QVERIFY(found);
        QCOMPARE(found->executable, QStringLiteral("/usr/local/bin/claude"));
    }

    void returnsNothingWhenNothingIsValid() {
        FakeProcessRunner runner;
        QVERIFY(!locate(runner));
    }
};

QTEST_GUILESS_MAIN(TestClaudeLocator)
#include "tst_claudelocator.moc"
```

Append to `tests/CMakeLists.txt`:
```cmake

ecm_add_test(claudecode/tst_claudelocator.cpp
    TEST_NAME tst_claudelocator
    LINK_LIBRARIES contribusage_claudecode Qt6::Test)
target_include_directories(tst_claudelocator PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
```

- [ ] **Step 2: Run it and confirm it fails**

Run: `cmake -B build -G Ninja && cmake --build build`
Expected: FAIL with `claudecode/claudelocator.h: No such file or directory`.

- [ ] **Step 3: Write the locator**

`src/claudecode/claudelocator.h`:
```cpp
#pragma once

#include "core/process.h"

#include <QObject>
#include <QProcessEnvironment>
#include <QString>

#include <functional>
#include <optional>

namespace contribusage::claudecode {

struct FoundClaude {
    QString executable;
    QString version; // `claude --version`, trimmed
    // The widget's environment with PATH from the login shell (SPEC §8.1.1); nullopt inherits it. An npm install is a
    // `#!/usr/bin/env node` script, which fails without it when plasmashell's PATH lacks node.
    std::optional<QProcessEnvironment> environment;
};

// FR-6: finds the `claude` executable. Stateless; the caller caches the result and keeps the locator alive while a
// lookup runs.
class ClaudeLocator {
public:
    ClaudeLocator(ProcessRunning &runner, QString home, QString shell);

    // The first candidate in FR-6 order whose `--version` exits 0 within 10 s; an invalid override falls through.
    void locate(const QString &override, QObject *context, std::function<void(std::optional<FoundClaude>)> done);

private:
    void tryCandidates(QStringList candidates, std::optional<QProcessEnvironment> environment, QObject *context,
                       std::function<void(std::optional<FoundClaude>)> done);

    ProcessRunning &m_runner;
    QString m_home;
    QString m_shell;
};

} // namespace contribusage::claudecode
```

`src/claudecode/claudelocator.cpp`:
```cpp
#include "claudecode/claudelocator.h"

#include <QStringList>

namespace contribusage::claudecode {

ClaudeLocator::ClaudeLocator(ProcessRunning &runner, QString home, QString shell)
    : m_runner(runner), m_home(std::move(home)), m_shell(std::move(shell)) {}

void ClaudeLocator::locate(const QString &override, QObject *context,
                           std::function<void(std::optional<FoundClaude>)> done) {
    // One login shell run for both answers. PATH comes last so profile output only precedes it; the line before counts
    // as `claude` only when it is an absolute path (an alias prints `alias claude=…`, a miss prints nothing).
    const ProcessRequest lookup{
        .executable = m_shell,
        .arguments = {QStringLiteral("-lc"), QStringLiteral("command -v claude; printf '%s\\n' \"$PATH\"")},
        .workingDirectory = m_home,
        .environment = std::nullopt,
        .timeoutMs = 10'000};
    m_runner.run(lookup, context, [this, override, context, done = std::move(done)](ProcessOutcome outcome) mutable {
        QString fromShell;
        std::optional<QProcessEnvironment> environment;
        if (outcome && outcome->exitCode == 0) {
            const QStringList lines = outcome->stdoutText.split(u'\n', Qt::SkipEmptyParts);
            if (!lines.isEmpty()) {
                QProcessEnvironment withPath = QProcessEnvironment::systemEnvironment();
                withPath.insert(QStringLiteral("PATH"), lines.last());
                environment = withPath;
                if (lines.size() >= 2 && lines.at(lines.size() - 2).startsWith(u'/')) {
                    fromShell = lines.at(lines.size() - 2);
                }
            }
        }
        QStringList candidates;
        for (const QString &candidate :
             {override, fromShell, m_home + QStringLiteral("/.local/bin/claude"),
              m_home + QStringLiteral("/.npm-global/bin/claude"), QStringLiteral("/usr/local/bin/claude"),
              QStringLiteral("/usr/bin/claude")}) {
            if (!candidate.isEmpty() && !candidates.contains(candidate)) {
                candidates.append(candidate);
            }
        }
        tryCandidates(candidates, environment, context, std::move(done));
    });
}

void ClaudeLocator::tryCandidates(QStringList candidates, std::optional<QProcessEnvironment> environment,
                                  QObject *context, std::function<void(std::optional<FoundClaude>)> done) {
    if (candidates.isEmpty()) {
        done(std::nullopt);
        return;
    }
    const QString candidate = candidates.takeFirst();
    const ProcessRequest request{.executable = candidate,
                                 .arguments = {QStringLiteral("--version")},
                                 .workingDirectory = m_home,
                                 .environment = environment,
                                 .timeoutMs = 10'000};
    m_runner.run(request, context,
                 [this, candidate, candidates, environment, context, done = std::move(done)](ProcessOutcome outcome) mutable {
                     if (outcome && outcome->exitCode == 0) {
                         done(FoundClaude{.executable = candidate,
                                          .version = outcome->stdoutText.trimmed(),
                                          .environment = environment});
                         return;
                     }
                     tryCandidates(candidates, environment, context, std::move(done));
                 });
}

} // namespace contribusage::claudecode
```

In `src/claudecode/CMakeLists.txt`, add `claudelocator.cpp claudelocator.h` to the library's sources (before `usageparser.cpp`).

- [ ] **Step 4: Run the tests and confirm they pass**

Run: `cmake -B build -G Ninja && cmake --build build && ctest --test-dir build --output-on-failure -R tst_claudelocator`
Expected: PASS.

- [ ] **Step 5: Format, full suite, commit**

Run: `clang-format -i src/claudecode/* tests/claudecode/*.cpp tests/support/*.h && clang-format --dry-run --Werror $(git ls-files -co --exclude-standard '*.cpp' '*.h') && QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure`
Expected: no format output; all tests pass.

```bash
git add src/claudecode tests
git commit -m "Claude Code: locate claude through the login shell and known Linux paths

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 3: The probe and its error mapping

**Files:**
- Create: `src/claudecode/claudecodelimits.h`, `src/claudecode/claudecodelimits.cpp`, `tests/claudecode/tst_claudecodelimits.cpp`
- Modify: `src/claudecode/CMakeLists.txt`, `tests/CMakeLists.txt`

**Interfaces:**
- Consumes: `ClaudeLocator`, `FoundClaude` (Task 2), `parseUsage` (Task 1), `AppPaths`, `jsonstore::createFolder` (L-1a).
- Produces:
  - `using LimitsOutcome = std::expected<LimitsReport, SourceError>;`
  - `class ClaudeCodeLimits`:
    - constructor `ClaudeCodeLimits(ProcessRunning &runner, ClaudeLocator &locator, AppPaths paths, std::function<QDateTime()> now = &QDateTime::currentDateTime)`
    - `QString probeFolder() const`
    - `void setOverride(const QString &path)`
    - `void fetch(QObject *context, std::function<void(LimitsOutcome)> done)`

- [ ] **Step 1: Write the failing test**

`tests/claudecode/tst_claudecodelimits.cpp`:
```cpp
#include "claudecode/claudecodelimits.h"
#include "support/fakeprocessrunner.h"

#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QTemporaryDir>
#include <QTest>
#include <QTimer>

using namespace contribusage;
using namespace contribusage::claudecode;

namespace {

const QString shell = QStringLiteral("/usr/bin/zsh");
const QString claude = QStringLiteral("/home/octocat/.local/bin/claude");

QString fixture(const char *name) {
    QFile file(QStringLiteral(CONTRIBUSAGE_FIXTURES "/usage/") + QString::fromLatin1(name) + QStringLiteral(".txt"));
    return file.open(QIODevice::ReadOnly) ? QString::fromUtf8(file.readAll()) : QString();
}

// Finds claude through the login shell.
void scriptLocator(FakeProcessRunner &runner) {
    runner.answer(shell, {}, FakeProcessRunner::exited(0, claude + QStringLiteral("\n/usr/bin\n")));
    runner.answer(claude, QStringLiteral("--version"), FakeProcessRunner::exited(0, QStringLiteral("2.1.289 (Claude Code)\n")));
}

LimitsOutcome fetch(ClaudeCodeLimits &limits) {
    std::optional<LimitsOutcome> outcome;
    QEventLoop loop;
    limits.fetch(nullptr, [&](LimitsOutcome result) {
        outcome = std::move(result);
        loop.quit();
    });
    if (!outcome) {
        QTimer::singleShot(5'000, &loop, &QEventLoop::quit);
        loop.exec();
    }
    return outcome.value_or(std::unexpected(SourceError{.kind = SourceError::Kind::Io, .text = QStringLiteral("no answer")}));
}

} // namespace

// FR-7, P-10, FR-9 and SPEC §13, ported from ClaudeCodeProviderTests.swift.
class TestClaudeCodeLimits : public QObject {
    Q_OBJECT

private Q_SLOTS:
    // The hard rule: exactly these arguments, in the empty probe folder, 30 s.
    void probesWithTheExactArgumentsInTheProbeFolder() {
        QTemporaryDir temporary;
        FakeProcessRunner runner;
        scriptLocator(runner);
        runner.answer(claude, QStringLiteral("-p"), FakeProcessRunner::exited(0, fixture("subscription-basic")));
        ClaudeLocator locator(runner, QStringLiteral("/home/octocat"), shell);
        ClaudeCodeLimits limits(runner, locator, AppPaths{.root = temporary.path()},
                                [] { return QDateTime(QDate(2026, 9, 27), QTime(20, 0), QTimeZone::UTC); });

        const LimitsOutcome outcome = fetch(limits);

        QVERIFY(outcome);
        QCOMPARE(outcome->windows.size(), 2);
        const ProcessRequest &probe = runner.requests.last();
        QCOMPARE(probe.executable, claude);
        QCOMPARE(probe.arguments, (QStringList{QStringLiteral("-p"), QStringLiteral("/usage"),
                                               QStringLiteral("--no-session-persistence")}));
        QCOMPARE(probe.workingDirectory, temporary.path() + QStringLiteral("/providers/claude-code/probe"));
        QCOMPARE(limits.probeFolder(), probe.workingDirectory);
        QVERIFY(QDir(probe.workingDirectory).exists());
        QVERIFY(QDir(probe.workingDirectory).isEmpty());
        QCOMPARE(probe.timeoutMs, 30'000);
        QCOMPARE(probe.environment->value(QStringLiteral("PATH")), QStringLiteral("/usr/bin"));
    }

    void mapsFailures_data() {
        QTest::addColumn<int>("exitCode");
        QTest::addColumn<QString>("stdoutText");
        QTest::addColumn<QString>("stderrText");
        QTest::addColumn<int>("kind");
        QTest::addColumn<QString>("text");
        const auto kind = [](SourceError::Kind value) { return int(value); };
        QTest::newRow("not logged in") << 1 << QString() << QStringLiteral("Please log in with /login")
                                       << kind(SourceError::Kind::NotLoggedIn) << QString();
        QTest::newRow("process failed") << 2 << QString() << QStringLiteral("boom")
                                        << kind(SourceError::Kind::ProcessFailed) << QStringLiteral("boom");
        QTest::newRow("API key billing") << 0 << fixture("not-subscription") << QString()
                                         << kind(SourceError::Kind::UnsupportedPlan)
                                         << QStringLiteral("Total cost:            $0.0000");
        QTest::newRow("no windows") << 0 << QStringLiteral("You are currently using your subscription\nNothing else\n")
                                    << QString() << kind(SourceError::Kind::Unparseable)
                                    << QStringLiteral("You are currently using your subscription\nNothing else\n");
    }
    void mapsFailures() {
        QFETCH(int, exitCode);
        QFETCH(QString, stdoutText);
        QFETCH(QString, stderrText);
        QFETCH(int, kind);
        QFETCH(QString, text);
        QTemporaryDir temporary;
        FakeProcessRunner runner;
        scriptLocator(runner);
        runner.answer(claude, QStringLiteral("-p"), FakeProcessRunner::exited(exitCode, stdoutText, stderrText));
        ClaudeLocator locator(runner, QStringLiteral("/home/octocat"), shell);
        ClaudeCodeLimits limits(runner, locator, AppPaths{.root = temporary.path()});

        const LimitsOutcome outcome = fetch(limits);

        QVERIFY(!outcome);
        QCOMPARE(int(outcome.error().kind), kind);
        QCOMPARE(outcome.error().text, text);
        if (outcome.error().kind == SourceError::Kind::ProcessFailed) {
            QCOMPARE(outcome.error().code, exitCode);
        }
    }

    void timeoutStaysTimedOut() {
        QTemporaryDir temporary;
        FakeProcessRunner runner;
        scriptLocator(runner);
        runner.answer(claude, QStringLiteral("-p"), std::unexpected(SourceError{.kind = SourceError::Kind::TimedOut}));
        ClaudeLocator locator(runner, QStringLiteral("/home/octocat"), shell);
        ClaudeCodeLimits limits(runner, locator, AppPaths{.root = temporary.path()});
        const LimitsOutcome outcome = fetch(limits);
        QVERIFY(!outcome);
        QCOMPARE(outcome.error().kind, SourceError::Kind::TimedOut);
    }

    void notFoundIsToolNotFound() {
        QTemporaryDir temporary;
        FakeProcessRunner runner; // no rules: the shell and every candidate fail
        ClaudeLocator locator(runner, QStringLiteral("/home/octocat"), shell);
        ClaudeCodeLimits limits(runner, locator, AppPaths{.root = temporary.path()});
        const LimitsOutcome outcome = fetch(limits);
        QVERIFY(!outcome);
        QCOMPARE(outcome.error().kind, SourceError::Kind::ToolNotFound);
    }

    // FR-6: a launch failure means the path stopped working; locate once more, then give up.
    void relocatesOnceWhenThePathStopsWorking() {
        QTemporaryDir temporary;
        FakeProcessRunner runner;
        scriptLocator(runner); // --version works, but no rule answers "-p": the probe fails to launch (Io)
        ClaudeLocator locator(runner, QStringLiteral("/home/octocat"), shell);
        ClaudeCodeLimits limits(runner, locator, AppPaths{.root = temporary.path()});

        const LimitsOutcome outcome = fetch(limits);

        QVERIFY(!outcome);
        QCOMPARE(outcome.error().kind, SourceError::Kind::ToolNotFound);
        qsizetype lookups = 0;
        qsizetype probes = 0;
        for (const auto &request : runner.requests) {
            lookups += request.executable == shell;
            probes += request.arguments.value(0) == u"-p";
        }
        QCOMPARE(lookups, 2);
        QCOMPARE(probes, 2);
    }

    void cachesTheLocatedPath() {
        QTemporaryDir temporary;
        FakeProcessRunner runner;
        scriptLocator(runner);
        runner.answer(claude, QStringLiteral("-p"), FakeProcessRunner::exited(0, fixture("subscription-basic")));
        ClaudeLocator locator(runner, QStringLiteral("/home/octocat"), shell);
        ClaudeCodeLimits limits(runner, locator, AppPaths{.root = temporary.path()});
        QVERIFY(fetch(limits));
        QVERIFY(fetch(limits));
        qsizetype lookups = 0;
        for (const auto &request : runner.requests) {
            lookups += request.executable == shell;
        }
        QCOMPARE(lookups, 1);
    }
};

QTEST_GUILESS_MAIN(TestClaudeCodeLimits)
#include "tst_claudecodelimits.moc"
```

Append to `tests/CMakeLists.txt`:
```cmake

ecm_add_test(claudecode/tst_claudecodelimits.cpp
    TEST_NAME tst_claudecodelimits
    LINK_LIBRARIES contribusage_claudecode Qt6::Test)
target_include_directories(tst_claudecodelimits PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_definitions(tst_claudecodelimits PRIVATE
    CONTRIBUSAGE_FIXTURES="${CMAKE_CURRENT_SOURCE_DIR}/claudecode/fixtures")
```

- [ ] **Step 2: Run it and confirm it fails**

Run: `cmake -B build -G Ninja && cmake --build build`
Expected: FAIL with `claudecode/claudecodelimits.h: No such file or directory`.

- [ ] **Step 3: Write the implementation**

`src/claudecode/claudecodelimits.h`:
```cpp
#pragma once

#include "claudecode/claudelocator.h"
#include "core/apppaths.h"
#include "core/reports.h"

#include <QDateTime>

#include <expected>
#include <functional>
#include <optional>

namespace contribusage::claudecode {

using LimitsOutcome = std::expected<LimitsReport, SourceError>;

// FR-7: the `/usage` probe with SPEC §8.1.1's exact invocation, and the outcome as SPEC §13 names it. Keeps the
// located `claude` until a launch fails (FR-6). The caller keeps this object alive while a fetch runs and passes a
// context that dies with it.
class ClaudeCodeLimits {
public:
    ClaudeCodeLimits(ProcessRunning &runner, ClaudeLocator &locator, AppPaths paths,
                     std::function<QDateTime()> now = &QDateTime::currentDateTime);

    QString probeFolder() const;
    void setOverride(const QString &path);
    void fetch(QObject *context, std::function<void(LimitsOutcome)> done);

private:
    void probe(int attempt, QObject *context, std::function<void(LimitsOutcome)> done);
    LimitsOutcome reportFrom(const ProcessResult &result) const;

    ProcessRunning &m_runner;
    ClaudeLocator &m_locator;
    AppPaths m_paths;
    std::function<QDateTime()> m_now;
    QString m_override;
    std::optional<FoundClaude> m_located;
};

} // namespace contribusage::claudecode
```

`src/claudecode/claudecodelimits.cpp`:
```cpp
#include "claudecode/claudecodelimits.h"

#include "claudecode/usageparser.h"
#include "core/jsonstore.h"

#include <QRegularExpression>

namespace contribusage::claudecode {

ClaudeCodeLimits::ClaudeCodeLimits(ProcessRunning &runner, ClaudeLocator &locator, AppPaths paths,
                                   std::function<QDateTime()> now)
    : m_runner(runner), m_locator(locator), m_paths(std::move(paths)), m_now(std::move(now)) {}

QString ClaudeCodeLimits::probeFolder() const {
    return m_paths.providerFolder(providerId()) + QStringLiteral("/probe");
}

void ClaudeCodeLimits::setOverride(const QString &path) {
    if (path != m_override) {
        m_override = path;
        m_located.reset();
    }
}

void ClaudeCodeLimits::fetch(QObject *context, std::function<void(LimitsOutcome)> done) {
    if (!jsonstore::createFolder(probeFolder())) {
        done(std::unexpected(SourceError{.kind = SourceError::Kind::Io, .text = QStringLiteral("cannot create ") + probeFolder()}));
        return;
    }
    probe(1, context, std::move(done));
}

void ClaudeCodeLimits::probe(int attempt, QObject *context, std::function<void(LimitsOutcome)> done) {
    if (!m_located) {
        m_locator.locate(m_override, context, [this, attempt, context, done = std::move(done)](std::optional<FoundClaude> found) mutable {
            if (!found) {
                done(std::unexpected(SourceError{.kind = SourceError::Kind::ToolNotFound}));
                return;
            }
            m_located = std::move(found);
            probe(attempt, context, std::move(done));
        });
        return;
    }
    const ProcessRequest request{.executable = m_located->executable,
                                 .arguments = {QStringLiteral("-p"), QStringLiteral("/usage"),
                                               QStringLiteral("--no-session-persistence")},
                                 .workingDirectory = probeFolder(),
                                 .environment = m_located->environment,
                                 .timeoutMs = 30'000};
    m_runner.run(request, context, [this, attempt, context, done = std::move(done)](ProcessOutcome outcome) mutable {
        if (!outcome && outcome.error().kind == SourceError::Kind::Io) {
            // A launch failure: the path stopped working, for example after an update moved it (FR-6).
            m_located.reset();
            if (attempt == 1) {
                probe(2, context, std::move(done));
            } else {
                done(std::unexpected(SourceError{.kind = SourceError::Kind::ToolNotFound}));
            }
            return;
        }
        if (!outcome) {
            done(std::unexpected(outcome.error()));
            return;
        }
        done(reportFrom(*outcome));
    });
}

LimitsOutcome ClaudeCodeLimits::reportFrom(const ProcessResult &result) const {
    if (result.exitCode != 0) {
        // "login", "log in", "logged in"
        static const QRegularExpression login(QStringLiteral("\\blog(ged)? ?in\\b"),
                                              QRegularExpression::CaseInsensitiveOption);
        if (login.match(result.stdoutText + u'\n' + result.stderrText).hasMatch()) {
            return std::unexpected(SourceError{.kind = SourceError::Kind::NotLoggedIn});
        }
        return std::unexpected(SourceError{
            .kind = SourceError::Kind::ProcessFailed, .text = result.stderrText.right(500), .code = result.exitCode});
    }
    const LimitsReport report = parseUsage(result.stdoutText, m_now());
    if (report.billingNote && !report.billingNote->contains(QStringLiteral("subscription"), Qt::CaseInsensitive)) {
        return std::unexpected(SourceError{.kind = SourceError::Kind::UnsupportedPlan, .text = *report.billingNote}); // P-10
    }
    if (report.windows.isEmpty()) {
        return std::unexpected(SourceError{.kind = SourceError::Kind::Unparseable, .text = result.stdoutText}); // FR-9
    }
    return report;
}

} // namespace contribusage::claudecode
```

In `src/claudecode/CMakeLists.txt`, add `claudecodelimits.cpp claudecodelimits.h` to the sources (first in the list).

- [ ] **Step 4: Run the tests and confirm they pass**

Run: `cmake -B build -G Ninja && cmake --build build && ctest --test-dir build --output-on-failure -R tst_claudecodelimits`
Expected: PASS.

- [ ] **Step 5: Format, full suite, commit**

Run: `clang-format -i src/claudecode/* tests/claudecode/*.cpp && clang-format --dry-run --Werror $(git ls-files -co --exclude-standard '*.cpp' '*.h') && QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure`
Expected: no format output; all tests pass.

```bash
git add src/claudecode tests
git commit -m "Claude Code: the /usage probe and its error mapping

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 4: The last limits snapshot across restarts (FR-10)

**Files:**
- Create: `src/core/limitscache.h`, `src/core/limitscache.cpp`, `tests/core/tst_limitscache.cpp`
- Modify: `src/core/CMakeLists.txt`, `tests/CMakeLists.txt`

**Interfaces:**
- Consumes: `jsonstore`, `AppPaths`, `Snapshot`, `toJson(LimitsReport)`, `limitsReportFromJson` (L-1a).
- Produces (namespace `contribusage::limitscache`, header `core/limitscache.h`): `std::optional<Snapshot<LimitsReport>> load(const AppPaths &, const QString &providerId)`, `bool save(const AppPaths &, const Snapshot<LimitsReport> &)`; constant `int schemaVersion = 1`.

- [ ] **Step 1: Write the failing test**

`tests/core/tst_limitscache.cpp`:
```cpp
#include "core/jsonstore.h"
#include "core/limitscache.h"

#include <QFile>
#include <QTemporaryDir>
#include <QTest>
#include <QTimeZone>

using namespace contribusage;

namespace {

Snapshot<LimitsReport> snapshot(const QString &provider, double percent) {
    return {.value = LimitsReport{.provider = provider,
                                  .windows = {UsageWindow{.label = QStringLiteral("Current session"),
                                                          .kind = WindowKind::Session,
                                                          .usedPercent = percent,
                                                          .isBelowOne = false,
                                                          .resetsAt = std::nullopt}},
                                  .billingNote = std::nullopt,
                                  .insights = std::nullopt,
                                  .rawOutput = std::nullopt},
            .fetchedAt = QDateTime(QDate(2026, 10, 5), QTime(1, 0), QTimeZone::UTC),
            .origin = origins::poll()};
}

} // namespace

// FR-10, SPEC §10.7: state.json holds the last limits snapshot per provider; it is a cache.
class TestLimitsCache : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void roundTrips() {
        QTemporaryDir temporary;
        const AppPaths paths{.root = temporary.path()};
        QVERIFY(!limitscache::load(paths, QStringLiteral("claude-code")));
        QVERIFY(limitscache::save(paths, snapshot(QStringLiteral("claude-code"), 23)));
        const auto loaded = limitscache::load(paths, QStringLiteral("claude-code"));
        QVERIFY(loaded);
        QVERIFY(*loaded == snapshot(QStringLiteral("claude-code"), 23));
        QVERIFY(QFile::exists(temporary.path() + QStringLiteral("/state.json")));
    }

    void keepsOtherProviders() {
        QTemporaryDir temporary;
        const AppPaths paths{.root = temporary.path()};
        QVERIFY(limitscache::save(paths, snapshot(QStringLiteral("claude-code"), 23)));
        QVERIFY(limitscache::save(paths, snapshot(QStringLiteral("other"), 50)));
        QVERIFY(limitscache::save(paths, snapshot(QStringLiteral("claude-code"), 30)));
        QCOMPARE(limitscache::load(paths, QStringLiteral("other"))->value.windows.at(0).usedPercent, 50.0);
        QCOMPARE(limitscache::load(paths, QStringLiteral("claude-code"))->value.windows.at(0).usedPercent, 30.0);
    }

    // A cache of another version or a broken one reads as empty, and the next save replaces it.
    void anotherVersionReadsAsEmptyAndIsReplaced() {
        QTemporaryDir temporary;
        const AppPaths paths{.root = temporary.path()};
        QVERIFY(jsonstore::write(paths.root + QStringLiteral("/state.json"), 99, QJsonObject()));
        QVERIFY(!limitscache::load(paths, QStringLiteral("claude-code")));
        QVERIFY(limitscache::save(paths, snapshot(QStringLiteral("claude-code"), 23)));
        QVERIFY(limitscache::load(paths, QStringLiteral("claude-code")));
    }
};

QTEST_GUILESS_MAIN(TestLimitsCache)
#include "tst_limitscache.moc"
```

Append to `tests/CMakeLists.txt`:
```cmake

ecm_add_test(core/tst_limitscache.cpp
    TEST_NAME tst_limitscache
    LINK_LIBRARIES contribusage_core Qt6::Test)
```

- [ ] **Step 2: Run it and confirm it fails**

Run: `cmake -B build -G Ninja && cmake --build build`
Expected: FAIL with `core/limitscache.h: No such file or directory`.

- [ ] **Step 3: Write the implementation**

`src/core/limitscache.h`:
```cpp
#pragma once

#include "core/apppaths.h"
#include "core/reports.h"
#include "core/snapshot.h"

#include <optional>

namespace contribusage::limitscache {

// state.json: {"limits": {"<provider>": {"fetchedAt": …, "origin": …, "report": …}}} (SPEC §10.7). Other keys (the
// GitHub snapshot, notification keys) join it in later milestones and are kept on save.
inline constexpr int schemaVersion = 1;

// FR-10: the last successful limits snapshot; nullopt when there is none or the cache is unreadable.
std::optional<Snapshot<LimitsReport>> load(const AppPaths &paths, const QString &providerId);

// Replaces this provider's snapshot and keeps everything else; an unreadable cache is started afresh.
bool save(const AppPaths &paths, const Snapshot<LimitsReport> &snapshot);

} // namespace contribusage::limitscache
```

`src/core/limitscache.cpp`:
```cpp
#include "core/limitscache.h"

#include "core/jsonstore.h"

#include <QJsonObject>

namespace contribusage::limitscache {

namespace {

QString file(const AppPaths &paths) {
    return paths.root + QStringLiteral("/state.json");
}

QJsonObject state(const AppPaths &paths) {
    const auto read = jsonstore::read(file(paths), schemaVersion);
    return read && read->has_value() ? read->value().toObject() : QJsonObject();
}

} // namespace

std::optional<Snapshot<LimitsReport>> load(const AppPaths &paths, const QString &providerId) {
    const QJsonObject entry = state(paths).value(QStringLiteral("limits")).toObject().value(providerId).toObject();
    const QDateTime fetchedAt =
        QDateTime::fromString(entry.value(QStringLiteral("fetchedAt")).toString(), Qt::ISODateWithMs);
    const auto report = limitsReportFromJson(entry.value(QStringLiteral("report")).toObject());
    if (!fetchedAt.isValid() || !report) {
        return std::nullopt;
    }
    return Snapshot<LimitsReport>{
        .value = *report, .fetchedAt = fetchedAt, .origin = entry.value(QStringLiteral("origin")).toString()};
}

bool save(const AppPaths &paths, const Snapshot<LimitsReport> &snapshot) {
    QJsonObject root = state(paths);
    QJsonObject limits = root.value(QStringLiteral("limits")).toObject();
    limits.insert(snapshot.value.provider,
                  QJsonObject{{QStringLiteral("fetchedAt"), snapshot.fetchedAt.toUTC().toString(Qt::ISODateWithMs)},
                              {QStringLiteral("origin"), snapshot.origin},
                              {QStringLiteral("report"), toJson(snapshot.value)}});
    root.insert(QStringLiteral("limits"), limits);
    return jsonstore::write(file(paths), schemaVersion, root).has_value();
}

} // namespace contribusage::limitscache
```

In `src/core/CMakeLists.txt`, add `limitscache.cpp limitscache.h` (after `jsonstore.h`).

- [ ] **Step 4: Run the tests and confirm they pass**

Run: `cmake -B build -G Ninja && cmake --build build && ctest --test-dir build --output-on-failure -R tst_limitscache`
Expected: PASS.

- [ ] **Step 5: Format, full suite, commit**

Run: `clang-format -i src/core/limitscache.* tests/core/tst_limitscache.cpp && clang-format --dry-run --Werror $(git ls-files -co --exclude-standard '*.cpp' '*.h') && QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure`
Expected: no format output; all tests pass.

```bash
git add src/core tests
git commit -m "Core: last limits snapshot per provider in state.json (FR-10)

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 5: The job, the error texts and the view

**Files:**
- Create: `src/claudecode/limitsjob.h`, `src/claudecode/limitsjob.cpp`, `src/claudecode/limitsview.h`, `src/claudecode/limitsview.cpp`, `tests/claudecode/tst_limitsjob.cpp`, `tests/claudecode/tst_limitsview.cpp`
- Modify: `src/core/display.h`, `src/core/display.cpp`, `tests/core/tst_display.cpp`, `src/claudecode/CMakeLists.txt`, `tests/CMakeLists.txt`

**Interfaces:**
- Consumes: Tasks 1 to 4, and `display.h` (L-4a).
- Produces:
  - In `core/display.h`: `QString meterLevelName(MeterLevel)` (`"normal"`, `"warning"` or `"critical"`) and `QString errorMessage(const SourceError &, const QString &displayName)` (SPEC §13).
  - `class LimitsJob : public QObject` (header `claudecode/limitsjob.h`):
    - constructor `LimitsJob(ClaudeCodeLimits &limits, AppPaths paths, int intervalMs, QObject *parent = nullptr)`
    - `const SourceState<LimitsReport> &state() const`
    - `void start()` and `void refresh()`
    - signal `stateChanged()`
  - `QVariantMap limitsView(const SourceState<LimitsReport> &state, const QDateTime &now)` (header `claudecode/limitsview.h`). Its keys:
    - `name`, `symbol`, `updatedText`, `statusText` and `stale` (a bool)
    - `windows`: the L-4a window maps, with ADR-042 applied
    - `insights`: present only when the report has insights; the L-4a shape, with rankings cut to their top three
  - `QVariantMap labelView(const SourceState<LimitsReport> &state, const QDateTime &now)`: `text`, `fraction` and `level` of the session window, or `?`, `0` and `normal` when there is none.

- [ ] **Step 1: Write the failing tests**

Append these slots to `TestDisplay` in `tests/core/tst_display.cpp` (before the closing `};`):
```cpp
    void meterLevelNames() {
        QCOMPARE(meterLevelName(MeterLevel::Normal), QStringLiteral("normal"));
        QCOMPARE(meterLevelName(MeterLevel::Warning), QStringLiteral("warning"));
        QCOMPARE(meterLevelName(MeterLevel::Critical), QStringLiteral("critical"));
    }

    // SPEC §13, worded once with the source's name.
    void errorMessages() {
        const QString name = QStringLiteral("Claude Code");
        QCOMPARE(errorMessage({.kind = SourceError::Kind::ToolNotFound}, name), QStringLiteral("Claude Code wasn't found"));
        QCOMPARE(errorMessage({.kind = SourceError::Kind::NotLoggedIn}, name), QStringLiteral("Claude Code isn't logged in"));
        QCOMPARE(errorMessage({.kind = SourceError::Kind::TimedOut}, name),
                 QStringLiteral("Claude Code didn't answer in time"));
        QCOMPARE(errorMessage({.kind = SourceError::Kind::ProcessFailed, .code = 2}, name), QStringLiteral("Exited with code 2"));
        QCOMPARE(errorMessage({.kind = SourceError::Kind::Unparseable}, name), QStringLiteral("Couldn't read the usage output"));
        QCOMPARE(errorMessage({.kind = SourceError::Kind::Io, .text = QStringLiteral("disk full")}, name),
                 QStringLiteral("disk full"));
        QCOMPARE(errorMessage({.kind = SourceError::Kind::UnsupportedPlan, .text = QStringLiteral("note")}, name),
                 QStringLiteral("note"));
    }
```

`tests/claudecode/tst_limitsjob.cpp`:
```cpp
#include "claudecode/limitsjob.h"
#include "core/limitscache.h"
#include "support/fakeprocessrunner.h"

#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

using namespace contribusage;
using namespace contribusage::claudecode;

namespace {

const QString shell = QStringLiteral("/usr/bin/zsh");
const QString claude = QStringLiteral("/home/octocat/.local/bin/claude");

QString fixture(const char *name) {
    QFile file(QStringLiteral(CONTRIBUSAGE_FIXTURES "/usage/") + QString::fromLatin1(name) + QStringLiteral(".txt"));
    return file.open(QIODevice::ReadOnly) ? QString::fromUtf8(file.readAll()) : QString();
}

struct Fixture {
    QTemporaryDir temporary;
    FakeProcessRunner runner;
    ClaudeLocator locator{runner, QStringLiteral("/home/octocat"), shell};
    ClaudeCodeLimits limits{runner, locator, AppPaths{.root = temporary.path()}};

    Fixture() {
        runner.answer(shell, {}, FakeProcessRunner::exited(0, claude + QStringLiteral("\n/usr/bin\n")));
        runner.answer(claude, QStringLiteral("--version"), FakeProcessRunner::exited(0, QStringLiteral("2.1.289\n")));
    }
    AppPaths paths() const { return {.root = temporary.path()}; }
    qsizetype probes() const {
        qsizetype count = 0;
        for (const auto &request : runner.requests) {
            count += request.arguments.value(0) == u"-p";
        }
        return count;
    }
};

using Phase = SourceState<LimitsReport>::Phase;

} // namespace

// The probe on an interval, the cache (FR-10) and SPEC §11.3's states.
class TestLimitsJob : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void loadsAndCaches() {
        Fixture fixture;
        fixture.runner.answer(claude, QStringLiteral("-p"), FakeProcessRunner::exited(0, ::fixture("subscription-basic")));
        LimitsJob job(fixture.limits, fixture.paths(), 60'000);
        QSignalSpy changed(&job, &LimitsJob::stateChanged);
        job.start();
        QCOMPARE(job.state().phase, Phase::Loading);
        QTRY_COMPARE(job.state().phase, Phase::Loaded);
        QCOMPARE(job.state().snapshot()->value.windows.size(), 2);
        QVERIFY(limitscache::load(fixture.paths(), QStringLiteral("claude-code")));
        QVERIFY(changed.count() >= 2);
    }

    void startsWithTheCachedSnapshot() {
        Fixture fixture;
        const Snapshot<LimitsReport> cached{
            .value = LimitsReport{.provider = QStringLiteral("claude-code"), .windows = {}, .billingNote = {}, .insights = {},
                                  .rawOutput = {}},
            .fetchedAt = QDateTime::currentDateTimeUtc().addSecs(-3600),
            .origin = origins::poll()};
        QVERIFY(limitscache::save(fixture.paths(), cached));
        LimitsJob job(fixture.limits, fixture.paths(), 60'000);
        job.start();
        QCOMPARE(job.state().phase, Phase::Loading);
        QVERIFY(job.state().snapshot() == cached);
    }

    // A failed probe keeps the previous snapshot to show dimmed.
    void failureKeepsThePreviousSnapshot() {
        Fixture fixture;
        fixture.runner.answer(claude, QStringLiteral("-p"), FakeProcessRunner::exited(0, ::fixture("subscription-basic")));
        LimitsJob job(fixture.limits, fixture.paths(), 60'000);
        job.start();
        QTRY_COMPARE(job.state().phase, Phase::Loaded);
        fixture.runner.rules.last().outcome = std::unexpected(SourceError{.kind = SourceError::Kind::TimedOut});
        job.refresh();
        QTRY_COMPARE(job.state().phase, Phase::Failed);
        QCOMPARE(job.state().error->kind, SourceError::Kind::TimedOut);
        QVERIFY(job.state().snapshot());
    }

    void notFoundAndUnsupportedPlanAreNotConfigured() {
        Fixture missing;
        missing.runner.rules.clear();
        LimitsJob missingJob(missing.limits, missing.paths(), 60'000);
        missingJob.start();
        QTRY_COMPARE(missingJob.state().phase, Phase::NotConfigured);
        QCOMPARE(missingJob.state().reason, NotConfiguredReason::ToolNotInstalled);

        Fixture apiKey;
        apiKey.runner.answer(claude, QStringLiteral("-p"), FakeProcessRunner::exited(0, ::fixture("not-subscription")));
        LimitsJob apiKeyJob(apiKey.limits, apiKey.paths(), 60'000);
        apiKeyJob.start();
        QTRY_COMPARE(apiKeyJob.state().phase, Phase::NotConfigured);
        QCOMPARE(apiKeyJob.state().reason, NotConfiguredReason::UnsupportedPlan);
        QCOMPARE(apiKeyJob.state().note, QStringLiteral("Total cost:            $0.0000"));
    }

    void probesAgainOnTheInterval() {
        Fixture fixture;
        fixture.runner.answer(claude, QStringLiteral("-p"), FakeProcessRunner::exited(0, ::fixture("subscription-basic")));
        LimitsJob job(fixture.limits, fixture.paths(), 100);
        job.start();
        QTRY_VERIFY(fixture.probes() >= 3);
    }

    void aDeletedJobGetsNoLateResult() {
        Fixture fixture;
        fixture.runner.answer(claude, QStringLiteral("-p"), FakeProcessRunner::exited(0, ::fixture("subscription-basic")));
        auto *job = new LimitsJob(fixture.limits, fixture.paths(), 60'000);
        job->start();
        delete job;
        QTest::qWait(100); // the fake's answers arrive now and must not touch the deleted job
        QVERIFY(!limitscache::load(fixture.paths(), QStringLiteral("claude-code")));
    }
};

QTEST_GUILESS_MAIN(TestLimitsJob)
#include "tst_limitsjob.moc"
```

`tests/claudecode/tst_limitsview.cpp`:
```cpp
#include "claudecode/limitsview.h"

#include <QTest>
#include <QTimeZone>

using namespace contribusage;
using namespace contribusage::claudecode;

namespace {

const QDateTime now(QDate(2026, 10, 5), QTime(1, 0), QTimeZone::UTC);

Snapshot<LimitsReport> snapshot(qint64 ageSeconds) {
    const auto window = [](const char *label, WindowKind kind, double percent) {
        return UsageWindow{.label = QString::fromLatin1(label), .kind = kind, .usedPercent = percent, .isBelowOne = false,
                           .resetsAt = now.addSecs(7800)};
    };
    return {.value = LimitsReport{.provider = QStringLiteral("claude-code"),
                                  .windows = {window("Current session", WindowKind::Session, 23),
                                              window("Current week (all models)", WindowKind::Weekly, 74),
                                              window("Current week (Opus)", WindowKind::Weekly, 93)},
                                  .billingNote = std::nullopt,
                                  .insights = Insights{.note = QStringLiteral("Approximate."),
                                                       .periods = {InsightsPeriod{
                                                           .label = QStringLiteral("Last 24h"),
                                                           .summary = QStringLiteral("5 requests"),
                                                           .shares = {{.label = QStringLiteral("At >150k context"), .percent = 62}},
                                                           .rankings = {{.title = QStringLiteral("Skills"),
                                                                         .items = {{.label = QStringLiteral("/a"), .percent = 4},
                                                                                   {.label = QStringLiteral("/b"), .percent = 3},
                                                                                   {.label = QStringLiteral("/c"), .percent = 2},
                                                                                   {.label = QStringLiteral("/d"), .percent = 1}}}}}}},
                                  .rawOutput = std::nullopt},
            .fetchedAt = now.addSecs(-ageSeconds),
            .origin = origins::poll()};
}

} // namespace

class TestLimitsView : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void loadedShowsWindowsAndInsights() {
        const QVariantMap view = limitsView(SourceState<LimitsReport>::loaded(snapshot(120)), now);
        QCOMPARE(view.value(QStringLiteral("name")).toString(), QStringLiteral("Claude Code"));
        QCOMPARE(view.value(QStringLiteral("updatedText")).toString(), QStringLiteral("updated 2 min ago"));
        QCOMPARE(view.value(QStringLiteral("statusText")).toString(), QString());
        QVERIFY(!view.value(QStringLiteral("stale")).toBool());
        const QVariantList windows = view.value(QStringLiteral("windows")).toList();
        QCOMPARE(windows.size(), 2); // ADR-042: the Opus week is hidden
        QCOMPARE(windows.at(1).toMap().value(QStringLiteral("label")).toString(), QStringLiteral("Current week"));
        QCOMPARE(windows.at(1).toMap().value(QStringLiteral("level")).toString(), QStringLiteral("warning"));
        QCOMPARE(windows.at(0).toMap().value(QStringLiteral("resetText")).toString(),
                 QStringLiteral("resets in 2 hr 10 min"));
        const QVariantMap insights = view.value(QStringLiteral("insights")).toMap();
        const QVariantMap period = insights.value(QStringLiteral("periods")).toList().at(0).toMap();
        QCOMPARE(period.value(QStringLiteral("rows")).toList().at(0).toMap().value(QStringLiteral("percent")).toString(),
                 QStringLiteral("62%"));
        const QVariantMap skills = period.value(QStringLiteral("rankings")).toList().at(0).toMap();
        QCOMPARE(skills.value(QStringLiteral("rows")).toList().size(), 3); // FR-38: the top three
    }

    void staleAfterThirtyMinutes() {
        QVERIFY(limitsView(SourceState<LimitsReport>::loaded(snapshot(31 * 60)), now).value(QStringLiteral("stale")).toBool());
    }

    // SPEC §11.3's texts.
    void statusTexts() {
        const auto status = [](const SourceState<LimitsReport> &state) {
            return limitsView(state, now).value(QStringLiteral("statusText")).toString();
        };
        QCOMPARE(status(SourceState<LimitsReport>::loading(std::nullopt)), QStringLiteral("Loading…"));
        QCOMPARE(status(SourceState<LimitsReport>::loading(snapshot(60))), QString());
        QCOMPARE(status(SourceState<LimitsReport>::notConfigured(NotConfiguredReason::ToolNotInstalled)),
                 QStringLiteral("Claude Code not found"));
        QCOMPARE(status(SourceState<LimitsReport>::notConfigured(NotConfiguredReason::UnsupportedPlan, QStringLiteral("x"))),
                 QStringLiteral("Plan limits are only available when Claude Code uses a Claude subscription"));
        QCOMPARE(status(SourceState<LimitsReport>::failed({.kind = SourceError::Kind::NotLoggedIn}, snapshot(60))),
                 QStringLiteral("Claude Code isn't logged in"));
        const QVariantMap failed =
            limitsView(SourceState<LimitsReport>::failed({.kind = SourceError::Kind::TimedOut}, snapshot(60)), now);
        QCOMPARE(failed.value(QStringLiteral("windows")).toList().size(), 2); // previous values stay
        QVERIFY(failed.value(QStringLiteral("stale")).toBool());               // and are dimmed
    }

    void labelShowsTheSessionOrUnknown() {
        const QVariantMap loaded = labelView(SourceState<LimitsReport>::loaded(snapshot(60)), now);
        QCOMPARE(loaded.value(QStringLiteral("text")).toString(), QStringLiteral("23%"));
        QCOMPARE(loaded.value(QStringLiteral("fraction")).toDouble(), 0.23);
        const QVariantMap unknown = labelView(SourceState<LimitsReport>::loading(std::nullopt), now);
        QCOMPARE(unknown.value(QStringLiteral("text")).toString(), QStringLiteral("?"));
        QCOMPARE(unknown.value(QStringLiteral("fraction")).toDouble(), 0.0);
    }
};

QTEST_GUILESS_MAIN(TestLimitsView)
#include "tst_limitsview.moc"
```

Append to `tests/CMakeLists.txt`:
```cmake

ecm_add_test(claudecode/tst_limitsjob.cpp
    TEST_NAME tst_limitsjob
    LINK_LIBRARIES contribusage_claudecode Qt6::Test)
target_include_directories(tst_limitsjob PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_definitions(tst_limitsjob PRIVATE
    CONTRIBUSAGE_FIXTURES="${CMAKE_CURRENT_SOURCE_DIR}/claudecode/fixtures")

ecm_add_test(claudecode/tst_limitsview.cpp
    TEST_NAME tst_limitsview
    LINK_LIBRARIES contribusage_claudecode Qt6::Test)
```

- [ ] **Step 2: Run it and confirm it fails**

Run: `cmake -B build -G Ninja && cmake --build build`
Expected: FAIL with missing `meterLevelName`/`errorMessage` and missing `claudecode/limitsjob.h`.

- [ ] **Step 3: Write the implementation**

In `src/core/display.h`, add `#include "core/reports.h"` to the includes, and before the closing namespace add:
```cpp
// "normal", "warning", "critical": the names QML's Theme.meterColor reads.
QString meterLevelName(MeterLevel level);

// SPEC §13: one line for a failed source, worded with its name; technical detail is kept as the source reported it.
QString errorMessage(const SourceError &error, const QString &displayName);
```

In `src/core/display.cpp`, before the closing namespace add:
```cpp
QString meterLevelName(MeterLevel level) {
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

QString errorMessage(const SourceError &error, const QString &displayName) {
    using Kind = SourceError::Kind;
    switch (error.kind) {
    case Kind::ToolNotFound:
        return displayName + QStringLiteral(" wasn't found");
    case Kind::NotLoggedIn:
        return displayName + QStringLiteral(" isn't logged in");
    case Kind::UnsupportedPlan:
    case Kind::Decoding:
    case Kind::Io:
        return error.text;
    case Kind::TimedOut:
        return displayName + QStringLiteral(" didn't answer in time");
    case Kind::ProcessFailed:
        return QStringLiteral("Exited with code ") + QString::number(error.code);
    case Kind::Unparseable:
        return QStringLiteral("Couldn't read the usage output");
    case Kind::Offline:
        return QStringLiteral("Offline");
    case Kind::TokenMissing:
        return QStringLiteral("No ") + displayName + QStringLiteral(" token saved");
    case Kind::Unauthorized:
        return displayName + QStringLiteral(" token is invalid or expired");
    case Kind::RateLimited:
        return QStringLiteral("Rate limited until ") + QLocale().toString(error.until.toLocalTime().time(), QLocale::ShortFormat);
    case Kind::Http:
        return QStringLiteral("Server error ") + QString::number(error.code);
    case Kind::ProviderSpecific:
        break;
    }
    return QStringLiteral("Something went wrong with ") + displayName;
}
```

`src/claudecode/limitsjob.h`:
```cpp
#pragma once

#include "claudecode/claudecodelimits.h"
#include "core/snapshot.h"

#include <QObject>
#include <QTimer>

namespace contribusage::claudecode {

// The limits source on an interval (ADR-039), with FR-10's cache and SPEC §11.3's states. A stand-in for the refresh
// coordinator of L-1c, which takes over the schedule and its triggers.
class LimitsJob : public QObject {
    Q_OBJECT

public:
    LimitsJob(ClaudeCodeLimits &limits, AppPaths paths, int intervalMs, QObject *parent = nullptr);

    const SourceState<LimitsReport> &state() const { return m_state; }

    // Shows the cached snapshot, probes now, then every interval.
    void start();
    // Probes now unless a probe is running.
    void refresh();

Q_SIGNALS:
    void stateChanged();

private:
    void finish(LimitsOutcome outcome);

    ClaudeCodeLimits &m_limits;
    AppPaths m_paths;
    QTimer m_timer;
    bool m_fetching = false;
    SourceState<LimitsReport> m_state = SourceState<LimitsReport>::loading(std::nullopt);
};

} // namespace contribusage::claudecode
```

`src/claudecode/limitsjob.cpp`:
```cpp
#include "claudecode/limitsjob.h"

#include "claudecode/usageparser.h"
#include "core/limitscache.h"

namespace contribusage::claudecode {

LimitsJob::LimitsJob(ClaudeCodeLimits &limits, AppPaths paths, int intervalMs, QObject *parent)
    : QObject(parent), m_limits(limits), m_paths(std::move(paths)) {
    m_timer.setInterval(intervalMs);
    connect(&m_timer, &QTimer::timeout, this, &LimitsJob::refresh);
}

void LimitsJob::start() {
    m_state = SourceState<LimitsReport>::loading(limitscache::load(m_paths, providerId()));
    Q_EMIT stateChanged();
    refresh();
    m_timer.start();
}

void LimitsJob::refresh() {
    if (m_fetching) {
        return;
    }
    m_fetching = true;
    // `this` as context: a job deleted during a probe never gets its result (L-1a's guard).
    m_limits.fetch(this, [this](LimitsOutcome outcome) { finish(std::move(outcome)); });
}

void LimitsJob::finish(LimitsOutcome outcome) {
    m_fetching = false;
    if (outcome) {
        const Snapshot<LimitsReport> snapshot{
            .value = std::move(*outcome), .fetchedAt = QDateTime::currentDateTimeUtc(), .origin = origins::poll()};
        limitscache::save(m_paths, snapshot);
        m_state = SourceState<LimitsReport>::loaded(snapshot);
    } else if (outcome.error().kind == SourceError::Kind::ToolNotFound) {
        m_state = SourceState<LimitsReport>::notConfigured(NotConfiguredReason::ToolNotInstalled);
    } else if (outcome.error().kind == SourceError::Kind::UnsupportedPlan) {
        m_state = SourceState<LimitsReport>::notConfigured(NotConfiguredReason::UnsupportedPlan, outcome.error().text);
    } else {
        m_state = SourceState<LimitsReport>::failed(outcome.error(), m_state.snapshot());
    }
    Q_EMIT stateChanged();
}

} // namespace contribusage::claudecode
```

`src/claudecode/limitsview.h`:
```cpp
#pragma once

#include "core/reports.h"
#include "core/snapshot.h"

#include <QDateTime>
#include <QVariantMap>

namespace contribusage::claudecode {

// What QML reads for the Claude Code group (L-4a's shape): name, symbol, updatedText, statusText, stale, windows
// (ADR-042 applied) and, when the report has them, insights with each ranking cut to its top three (FR-38).
QVariantMap limitsView(const SourceState<LimitsReport> &state, const QDateTime &now);

// SPEC §11.1: the session window's text, fraction and level for the panel label; "?" without one.
QVariantMap labelView(const SourceState<LimitsReport> &state, const QDateTime &now);

} // namespace contribusage::claudecode
```

`src/claudecode/limitsview.cpp`:
```cpp
#include "claudecode/limitsview.h"

#include "core/display.h"

#include <QVariantList>

namespace contribusage::claudecode {

namespace {

constexpr qint64 staleAfterSeconds = 30 * 60; // SPEC §8.1.4

QVariantMap windowView(const UsageWindow &window, const QString &label, const QDateTime &now) {
    return {
        {QStringLiteral("label"), label},
        {QStringLiteral("percentText"), percentText(window.usedPercent, window.isBelowOne, window.resetsAt, now)},
        {QStringLiteral("fraction"), barFraction(window.usedPercent, window.resetsAt, now)},
        {QStringLiteral("level"), meterLevelName(meterLevel(window.usedPercent, window.resetsAt, now))},
        {QStringLiteral("resetText"), resetText(window.resetsAt, now).value_or(QString())},
    };
}

QVariantList rows(const QList<Share> &shares, qsizetype limit) {
    QVariantList list;
    for (const Share &share : shares.first(std::min(limit, shares.size()))) {
        list.append(QVariantMap{
            {QStringLiteral("label"), share.label},
            {QStringLiteral("percent"), share.percent ? QString(QString::number(*share.percent) + u'%') : QString()}});
    }
    return list;
}

QVariantMap insightsView(const Insights &insights) {
    QVariantList periods;
    for (const InsightsPeriod &period : insights.periods) {
        QVariantList rankings;
        for (const Ranking &ranking : period.rankings) {
            rankings.append(QVariantMap{{QStringLiteral("title"), ranking.title},
                                        {QStringLiteral("rows"), rows(ranking.items, 3)}});
        }
        periods.append(QVariantMap{{QStringLiteral("label"), period.label},
                                   {QStringLiteral("summary"), period.summary},
                                   {QStringLiteral("rows"), rows(period.shares, period.shares.size())},
                                   {QStringLiteral("rankings"), rankings}});
    }
    return {{QStringLiteral("note"), insights.note.value_or(QString())}, {QStringLiteral("periods"), periods}};
}

QString statusText(const SourceState<LimitsReport> &state) {
    using Phase = SourceState<LimitsReport>::Phase;
    switch (state.phase) {
    case Phase::Loading:
        return state.snapshot() ? QString() : QStringLiteral("Loading…");
    case Phase::NotConfigured:
        return state.reason == NotConfiguredReason::UnsupportedPlan
                   ? QStringLiteral("Plan limits are only available when Claude Code uses a Claude subscription")
                   : QStringLiteral("Claude Code not found");
    case Phase::Failed:
        return errorMessage(*state.error, QStringLiteral("Claude Code"));
    case Phase::Loaded:
        break;
    }
    return {};
}

} // namespace

QVariantMap limitsView(const SourceState<LimitsReport> &state, const QDateTime &now) {
    QVariantList windows;
    QString updated;
    bool stale = false;
    if (const auto snapshot = state.snapshot()) {
        for (const UsageWindow &window : snapshot->value.windows) {
            if (const auto label = shownWindowLabel(window.label)) {
                windows.append(windowView(window, *label, now));
            }
        }
        updated = ageText(snapshot->fetchedAt, now);
        stale = snapshot->isStale(now, staleAfterSeconds) || state.phase == SourceState<LimitsReport>::Phase::Failed;
    }
    QVariantMap view{{QStringLiteral("name"), QStringLiteral("Claude Code")},
                     {QStringLiteral("symbol"), QStringLiteral("draw-star")},
                     {QStringLiteral("updatedText"), updated},
                     {QStringLiteral("statusText"), statusText(state)},
                     {QStringLiteral("stale"), stale},
                     {QStringLiteral("windows"), windows}};
    if (const auto snapshot = state.snapshot(); snapshot && snapshot->value.insights) {
        view.insert(QStringLiteral("insights"), insightsView(*snapshot->value.insights));
    }
    return view;
}

QVariantMap labelView(const SourceState<LimitsReport> &state, const QDateTime &now) {
    if (const auto snapshot = state.snapshot()) {
        for (const UsageWindow &window : snapshot->value.windows) {
            if (window.kind == WindowKind::Session) {
                return {{QStringLiteral("text"), percentText(window.usedPercent, window.isBelowOne, window.resetsAt, now)},
                        {QStringLiteral("fraction"), barFraction(window.usedPercent, window.resetsAt, now)},
                        {QStringLiteral("level"), meterLevelName(meterLevel(window.usedPercent, window.resetsAt, now))}};
            }
        }
    }
    return {{QStringLiteral("text"), QStringLiteral("?")},
            {QStringLiteral("fraction"), 0.0},
            {QStringLiteral("level"), QStringLiteral("normal")}};
}

} // namespace contribusage::claudecode
```

In `src/claudecode/CMakeLists.txt`, add `limitsjob.cpp limitsjob.h limitsview.cpp limitsview.h` to the sources.

- [ ] **Step 4: Run the tests and confirm they pass**

Run: `cmake -B build -G Ninja && cmake --build build && QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure -R "tst_display|tst_limitsjob|tst_limitsview"`
Expected: PASS.

- [ ] **Step 5: Format, full suite, commit**

Run: `clang-format -i src/core/display.* src/claudecode/* tests/core/tst_display.cpp tests/claudecode/*.cpp && clang-format --dry-run --Werror $(git ls-files -co --exclude-standard '*.cpp' '*.h') && QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure`
Expected: no format output; all tests pass.

```bash
git add src tests
git commit -m "Claude Code: limits job with cache and states, and its view for QML

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 6: The widget shows the real limits

**Files:**
- Modify: `src/applet/popupmodel.h`, `src/applet/popupmodel.cpp`, `src/applet/CMakeLists.txt`, `src/applet/ProviderGroup.qml`, `src/applet/FullPopup.qml`, `tests/applet/tst_appletqml.cpp`

**Interfaces:**
- Consumes: `LiveProcessRunner` (L-1a), `ClaudeLocator`, `ClaudeCodeLimits`, `LimitsJob`, `limitsView`, `labelView` (Tasks 2 to 5), `meterLevelName` (Task 5).
- Produces: `PopupModel` properties `providers`, `heatmap`, `github`, `label`, now with the signal `changed()`. In live mode:
  - `heatmap` and `github` are empty maps, and the QML hides those blocks.
  - Each provider map lacks `activity`, and has `insights` only when the report has them.

- [ ] **Step 1: Write the failing test**

In `tests/applet/tst_appletqml.cpp`, change `initTestCase` so the model is created in fake data mode before anything loads the plugin:
```cpp
    void initTestCase() {
        // The applet tests read the macOS preview data; live mode would run the real claude.
        qputenv("CONTRIBUSAGE_FAKE_DATA", "1");
        QVERIFY2(m_loader.load(), qPrintable(m_loader.errorString()));
    }
```
and add this slot:
```cpp
    void popupModelNotifiesChanges() {
        QQmlEngine engine;
        auto *model = engine.singletonInstance<QObject *>(QString::fromLatin1(uri), QStringLiteral("PopupModel"));
        QVERIFY(model);
        QVERIFY(model->metaObject()->indexOfSignal("changed()") >= 0);
        const auto property = model->metaObject()->property(model->metaObject()->indexOfProperty("providers"));
        QVERIFY(property.hasNotifySignal());
    }
```

- [ ] **Step 2: Run it and confirm it fails**

Run: `cmake --build build && QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure -R tst_appletqml`
Expected: FAIL in `popupModelNotifiesChanges` (`indexOfSignal("changed()") >= 0` is false).

- [ ] **Step 3: Write the live mode**

`src/applet/popupmodel.h` becomes:
```cpp
#pragma once

#include <QObject>
#include <QTimer>
#include <QVariantList>
#include <QVariantMap>
#include <QtQml/qqmlregistration.h>

#include <memory>

namespace contribusage {
class LiveProcessRunner;
namespace claudecode {
class ClaudeLocator;
class ClaudeCodeLimits;
class LimitsJob;
} // namespace claudecode
} // namespace contribusage

// What the popup and the panel label show. Live mode runs the Claude Code limits probe (L-2a); blocks without a data
// source yet are left empty so QML hides them. CONTRIBUSAGE_FAKE_DATA=1 shows the macOS app's preview data instead
// (ADR-036's debug flag), which the applet tests use.
class PopupModel : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
    Q_PROPERTY(QVariantList providers READ providers NOTIFY changed)
    Q_PROPERTY(QVariantMap heatmap READ heatmap NOTIFY changed)
    Q_PROPERTY(QVariantMap github READ github NOTIFY changed)
    Q_PROPERTY(QVariantMap label READ label NOTIFY changed)

public:
    explicit PopupModel(QObject *parent = nullptr);
    ~PopupModel() override;

    QVariantList providers() const;
    QVariantMap heatmap() const { return m_heatmap; }
    QVariantMap github() const { return m_github; }
    QVariantMap label() const;

Q_SIGNALS:
    void changed();

private:
    void buildFakeData();

    bool m_fake = false;
    QVariantList m_providers; // fake mode
    QVariantMap m_heatmap;    // fake mode
    QVariantMap m_github;     // fake mode
    QVariantMap m_label;      // fake mode
    std::unique_ptr<contribusage::LiveProcessRunner> m_runner;
    std::unique_ptr<contribusage::claudecode::ClaudeLocator> m_locator;
    std::unique_ptr<contribusage::claudecode::ClaudeCodeLimits> m_limits;
    std::unique_ptr<contribusage::claudecode::LimitsJob> m_job;
    QTimer m_minute; // ages and countdowns move on (SPEC §11.2)
};
```

In `src/applet/popupmodel.cpp`:
1. Add the includes `#include "claudecode/claudecodelimits.h"`, `#include "claudecode/claudelocator.h"`, `#include "claudecode/limitsjob.h"`, `#include "claudecode/limitsview.h"`, `#include "core/apppaths.h"`, `#include "core/process.h"` and `#include <QDir>`.
2. Delete the local `levelName` function and replace its two uses with `meterLevelName`.
3. Rename the existing constructor body into `void PopupModel::buildFakeData()`. It starts with `const QDateTime now = QDateTime::currentDateTime();` exactly as the constructor did, and assigns `m_providers`, `m_heatmap`, `m_github` and `m_label`.
4. Add:
```cpp
PopupModel::PopupModel(QObject *parent) : QObject(parent) {
    m_fake = qgetenv("CONTRIBUSAGE_FAKE_DATA") == "1";
    if (m_fake) {
        buildFakeData();
        return;
    }
    const QString shell = qEnvironmentVariable("SHELL", QStringLiteral("/bin/sh"));
    m_runner = std::make_unique<LiveProcessRunner>();
    m_locator = std::make_unique<claudecode::ClaudeLocator>(*m_runner, QDir::homePath(), shell);
    m_limits = std::make_unique<claudecode::ClaudeCodeLimits>(*m_runner, *m_locator, AppPaths::live());
    m_job = std::make_unique<claudecode::LimitsJob>(*m_limits, AppPaths::live(), 5 * 60 * 1000); // ADR-039
    connect(m_job.get(), &claudecode::LimitsJob::stateChanged, this, &PopupModel::changed);
    m_minute.setInterval(60 * 1000);
    connect(&m_minute, &QTimer::timeout, this, &PopupModel::changed);
    m_minute.start();
    m_job->start();
}

// Out of line, where the owned types are complete; the job goes first, so no probe outlives its limits object.
PopupModel::~PopupModel() {
    m_job.reset();
}

QVariantList PopupModel::providers() const {
    if (m_fake) {
        return m_providers;
    }
    return {claudecode::limitsView(m_job->state(), QDateTime::currentDateTime())};
}

QVariantMap PopupModel::label() const {
    return m_fake ? m_label : claudecode::labelView(m_job->state(), QDateTime::currentDateTime());
}
```

In `src/applet/CMakeLists.txt`, change the link line to:
```cmake
target_link_libraries(org.contribusage.widget PRIVATE contribusage_claudecode contribusage_core Qt6::Qml Qt6::Quick)
```

`src/applet/ProviderGroup.qml`: under the `SectionHeader`, add the status line, dim the bars while stale, and load activity and insights only when present. Replace the `Repeater { … LimitBar … }`, `ActivitySection { … }` and `InsightsSection { … }` blocks with:
```qml
    PlasmaComponents.Label {
        visible: (provider.statusText ?? "").length > 0
        text: provider.statusText ?? ""
        wrapMode: Text.Wrap
        opacity: Theme.secondaryOpacity
        Layout.fillWidth: true
    }
    Repeater {
        model: provider.windows
        LimitBar {
            required property var modelData
            limit: modelData
            opacity: provider.stale ? 0.5 : 1
            Layout.fillWidth: true
        }
    }
    Loader {
        active: provider.activity !== undefined
        visible: active
        Layout.fillWidth: true
        sourceComponent: ActivitySection { activity: provider.activity }
    }
    Loader {
        active: provider.insights !== undefined
        visible: active
        Layout.fillWidth: true
        sourceComponent: InsightsSection { insights: provider.insights }
    }
```
and add `import org.kde.plasma.components as PlasmaComponents` to its imports.

`src/applet/FullPopup.qml`: replace the `HeatmapBlock { … }` and `GitHubSection { … }` blocks and their preceding dividers with:
```qml
    Rectangle {
        visible: heatmapLoader.active
        Layout.fillWidth: true
        implicitHeight: 1
        color: Qt.alpha(Kirigami.Theme.textColor, Theme.dividerOpacity)
    }
    Loader {
        id: heatmapLoader
        active: PopupModel.heatmap.cells !== undefined
        visible: active
        Layout.fillWidth: true
        Layout.margins: Theme.padding
        sourceComponent: HeatmapBlock { heatmap: PopupModel.heatmap }
    }
    Rectangle {
        visible: githubLoader.active
        Layout.fillWidth: true
        implicitHeight: 1
        color: Qt.alpha(Kirigami.Theme.textColor, Theme.dividerOpacity)
    }
    Loader {
        id: githubLoader
        active: PopupModel.github.login !== undefined
        visible: active
        Layout.fillWidth: true
        Layout.margins: Theme.padding
        sourceComponent: GitHubSection { github: PopupModel.github }
    }
```
(the divider before the footer stays).

- [ ] **Step 4: Run the tests and confirm they pass**

Run: `cmake -B build -G Ninja && cmake --build build && QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure`
Expected: all tests pass. The fake-mode tests of L-4a stay green.

- [ ] **Step 5: Look at it with your real limits**

Close the old viewer windows (`for p in $(pgrep -x plasmoidviewer); do kill $p; done`). Then run `QT_PLUGIN_PATH=$PWD/build/bin plasmoidviewer -a org.contribusage.widget -f planar -s 420x900` and take a screenshot with spectacle after 10 s (the first probe takes a few seconds).

Expected:
- the Claude Code header with "updated just now"
- the session bar and one week bar, with the same percentages as `claude -p /usage --no-session-persistence` prints, and reset texts that are not empty
- the Insights row
- the footer
- no activity, heatmap or GitHub blocks
- no QML warnings

Then check the panel label with `plasmoidviewer -c org.kde.panel -f horizontal -l topedge -s 500x220`. Expected: the mark plus the real session percentage.

Also confirm that `~/.local/share/contribusage/state.json` exists, and that `~/.local/share/contribusage/providers/claude-code/probe/` is empty.

- [ ] **Step 6: Format and commit**

```bash
clang-format -i src/applet/*.cpp src/applet/*.h tests/applet/*.cpp
git add src/applet tests
git commit -m "Applet: real Claude Code limits; mock data only under CONTRIBUSAGE_FAKE_DATA

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 7: Contract and vault

**Files:**
- Modify: `SPEC.md` (FR-6, §8.1.1, P-7, §17.8), `llm-wiki/modules/claude-code.md`, `llm-wiki/modules/applet.md`, `llm-wiki/architecture/repo-map.md`, `llm-wiki/log.md`

- [ ] **Step 1: Update the contract**

In `SPEC.md`:
- **FR-6:** replace the resolution order's (3) clause (`known locations on Apple Silicon: …`) with `(3) known locations on Linux: ~/.local/bin/claude (native installer), ~/.npm-global/bin/claude, /usr/local/bin/claude, /usr/bin/claude`.
- **§8.1.1:** replace the `cwd:` line with `cwd:          $XDG_DATA_HOME/contribusage/providers/claude-code/probe/   (empty directory, created on demand)`.
- **P-7:** replace the list of formats with ``` `MMM d 'at' h:mm a`, `MMM d 'at' h a`, `MMM d, h:mm a`, `MMM d, h a`, `MMM d`, `h:mm a`, `h a` ``` and append ` The comma forms appear from Claude Code 2.1.289 ("resets Oct 5, 6am"; fixture subscription-comma-dates.txt).`
- **§17.8:** replace the `L-2` line with:
```markdown
- [x] **L-2a** Claude Code limits live: locate `claude` (FR-6, Linux paths), the `/usage` probe every 5 min (FR-7, ADR-039), the parser with the comma reset format of 2.1.289, the error mapping (P-10, FR-9, §13), the last snapshot in `state.json` (FR-10), and the widget showing the real bars, insights and status; blocks without data hidden in live mode, the preview data under `CONTRIBUSAGE_FAKE_DATA=1`. Accept: `tst_usageparser`, `tst_claudelocator`, `tst_claudecodelimits`, `tst_limitscache`, `tst_limitsjob` and `tst_limitsview` pass; the viewer shows the same percentages as `claude -p /usage`.
- [ ] **L-2b** Claude Code activity from local transcripts (needs L-1b).
```

- [ ] **Step 2: Update the vault**

`llm-wiki/modules/claude-code.md`:
- set `status: active`, `updated:` today and `tracks: [src/claudecode, tests/claudecode]`
- replace the port note under the title with:
```markdown
> In C++ (`src/claudecode/`, target `contribusage_claudecode`, links only the core): `usageparser` (P-1 to P-11, plus the comma reset format of Claude Code 2.1.289), `claudelocator` (FR-6 with Linux paths, through the login shell), `claudecodelimits` (the exact SPEC §8.1.1 probe and its error mapping), `limitsjob` (5 min interval, `state.json` cache, SPEC §11.3 states; a stand-in until the refresh coordinator of L-1c) and `limitsview` (the state mapped for QML). Activity from transcripts follows in L-2b. Below: the Swift implementation at `macos-reference` (`2603a21`).
```

`llm-wiki/modules/applet.md`: set `updated:` today, and in `## Contract` replace `mock data until L-1 to L-3` with `real Claude Code limits and insights since L-2a; the macOS preview data under CONTRIBUSAGE_FAKE_DATA=1`. Add to `## Things that bite`:
```markdown
- In live mode the activity, heatmap and GitHub blocks are hidden until their data exists (L-2b, L-3); `CONTRIBUSAGE_FAKE_DATA=1 plasmoidviewer …` shows the full preview.
```

`llm-wiki/architecture/repo-map.md`: set `updated:` today and extend the `src/` line with `claudecode/: the Claude Code provider, contribusage_claudecode (parser, locator, probe, limits job, view);`.

`llm-wiki/log.md`, append:
```markdown

## [YYYY-MM-DD] feat | Real Claude Code limits (L-2a)
- `src/claudecode/`: parser, locator, probe, limits job and view; the widget shows the real limits and insights, cached in `state.json`.
- Claude Code 2.1.289 prints reset times as "Oct 5, 6am"; P-7 gained the comma forms.
```
(with today's date).

- [ ] **Step 3: Run every check and commit**

Run: `llm-wiki/tools/wiki.sh lint && cmake --build build && QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure && clang-format --dry-run --Werror $(git ls-files '*.cpp' '*.h') && echo all-ok`
Expected: `wiki lint: clean`, all tests pass, then `all-ok`.

```bash
git add SPEC.md llm-wiki
git commit -m "Spec: L-2a done; FR-6 Linux paths, comma reset format

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```
