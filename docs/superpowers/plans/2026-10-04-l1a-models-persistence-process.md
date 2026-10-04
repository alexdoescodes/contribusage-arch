# L-1a: Models, Persistence and the Process Runner Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Port the part of the Swift core that the Claude Code provider (L-2) needs first. That is the report models with their JSON form, snapshots and source states, the data folder with versioned atomic JSON files, and a non-blocking process runner. Each piece comes with its Swift tests ported to Qt Test.

**Architecture:** Plain C++23 value types and free functions in `contribusage_core`, which links only Qt6::Core. Errors are values (`std::expected`, `SourceError`), because KDE builds without exceptions. The process runner is asynchronous on Qt's event loop: `QProcess` signals, one shared FIFO queue so only one child process runs at a time (NFR-18), and a `QTimer` deadline. Nothing blocks the plasmashell GUI thread.

**Tech Stack:** C++23, Qt 6 Core (`QJsonDocument`, `QSaveFile`, `QProcess`, `QStandardPaths`, `QTimer`), Qt Test.

**Spec:** `docs/superpowers/specs/2026-10-04-linux-plasmoid-port-design.md` §1 (seams table, boundaries) and §2 (storage). Requirements: SPEC §10.1, §10.3, §10.4 (types), §10.6 (seams), §10.7 (persistence), FR-7 and NFR-18 (process). The Swift reference, read with `git show 2603a21:<path>`:
- sources: `Packages/ContribusageKit/Sources/ContribusageCore/Providers/Reports.swift`, `Models/Snapshot.swift`, `Persistence/JSONStore.swift`, `Support/AppPaths.swift`, `Support/Seams.swift` and `Support/Live/LiveProcessRunner.swift`
- tests: `Packages/ContribusageKit/Tests/ContribusageCoreTests/PersistenceTests.swift`, `SnapshotTests.swift` and `LiveProcessRunnerTests.swift`

L-1 is split into three plans: L-1a (this one), L-1b (file events, file reading seam, secret store, JSONL reader, history store) and L-1c (schedule, refresh coordinator, notification planner, popup layout, menu bar label).

## Global Constraints

- Everything from M0 and L-4a still holds: C++23, `-Werror`, clang-format clean, `QT_NO_CAST_FROM_ASCII` (string literals as `QStringLiteral`), and `src/core/` links only Qt6::Core.
- No exceptions (`-fno-exceptions`): failures are `std::expected<…, SourceError>` or `std::expected<…, StoreError>`.
- Data folder: `$XDG_DATA_HOME/contribusage` (normally `~/.local/share/contribusage`). New folders are created owner-only (`0700`). Every file the app writes is `{"schemaVersion": n, "value": …}` and is replaced atomically (port design §2, SPEC §10.7).
- Process runs (FR-7, NFR-18): stdin is `/dev/null`, both streams are captured, at most one child at a time across the whole process, and the timeout sends SIGTERM, then SIGKILL 2 s later. The runner never blocks: results arrive through a callback on the caller's thread.
- Tests use real temporary folders (`QTemporaryDir`) and real system tools (`/bin/sh`, `/bin/echo`, `/bin/sleep`, `/bin/cat`, `/usr/bin/false`), as the Swift tests did.

## Review Focus

1. **A child that ignores SIGTERM.** Expected: it is killed 2 s after the timeout, and the caller gets `TimedOut` in under 5 s. Pinned in Task 4 (`killsAProcessThatIgnoresSIGTERM`).
2. **Output larger than a pipe buffer on both streams.** Expected: no deadlock, all bytes arrive. Pinned in Task 4 (`readsLargeOutputOnBothStreams`).
3. **A data file of another schema version, or a broken file.** Expected: an error value, and the file is left untouched (a cache owner overwrites it on its next write, and `history.json` is never discarded). Pinned in Task 3 (`versionMismatchKeepsTheFile`, `malformedFileIsAnError`).
4. **A missing executable.** Expected: an `Io` error through the callback, and the queue moves on to the next request. Pinned in Task 4 (`missingExecutableFailsAndTheQueueMovesOn`).
5. **Report JSON from an older or foreign file.** Expected: `fromJson` returns `nullopt` for a missing or wrongly typed required field instead of a half-filled report. Pinned in Task 1 (`limitsReportRejectsAMissingProvider`, `limitsReportRejectsAnUnknownKind`).

---

## File Structure

| Path | Responsibility |
|---|---|
| `src/core/reports.h`, `src/core/reports.cpp` | `SourceError`, `UsageWindow`, `Insights`, `LimitsReport`, `TokenCounts`, `ActivityDay`, `ActivityReport`, and their JSON form |
| `src/core/snapshot.h` | `Snapshot<T>`, `SourceState<T>`, `NotConfiguredReason`, origins (header only, templates) |
| `src/core/apppaths.h`, `src/core/apppaths.cpp` | `AppPaths`: the data folder and provider folders |
| `src/core/jsonstore.h`, `src/core/jsonstore.cpp` | Versioned atomic JSON files, owner-only folders, provider data deletion |
| `src/core/process.h`, `src/core/process.cpp` | `ProcessRequest`, `ProcessResult`, the `ProcessRunning` seam, `LiveProcessRunner` |
| `tests/core/tst_reports.cpp`, `tst_snapshot.cpp`, `tst_jsonstore.cpp`, `tst_processrunner.cpp` | Their tests |

---

### Task 1: Report models and their JSON form

**Files:**
- Create: `src/core/reports.h`, `src/core/reports.cpp`, `tests/core/tst_reports.cpp`
- Modify: `src/core/CMakeLists.txt`, `tests/CMakeLists.txt`

**Interfaces:**
- Produces (namespace `contribusage`, header `core/reports.h`):
  - `struct SourceError { enum class Kind {ToolNotFound, NotLoggedIn, UnsupportedPlan, TimedOut, ProcessFailed, Unparseable, Offline, TokenMissing, Unauthorized, RateLimited, Http, Decoding, Io, ProviderSpecific}; Kind kind; QString text; int code; QDateTime until; }`
  - `enum class WindowKind { Session, Weekly, Other }`
  - `struct UsageWindow { QString label; WindowKind kind; double usedPercent; bool isBelowOne; std::optional<QDateTime> resetsAt; }`
  - `struct Share`, `struct Ranking`, `struct InsightsPeriod`, `struct Insights`, `struct LimitsReport`, `struct TokenCounts` (`total()`, `operator+=`), `struct ActivityDay`, `struct ActivityReport`. All of them have a defaulted `operator==`.
  - `QJsonObject toJson(const LimitsReport &)`, `std::optional<LimitsReport> limitsReportFromJson(const QJsonObject &)`, `QJsonObject toJson(const ActivityReport &)`, `std::optional<ActivityReport> activityReportFromJson(const QJsonObject &)`

- [ ] **Step 1: Write the failing test**

`tests/core/tst_reports.cpp`:
```cpp
#include "core/reports.h"

#include <QJsonArray>
#include <QTest>
#include <QTimeZone>

using namespace contribusage;

namespace {

LimitsReport sampleLimits() {
    return LimitsReport{
        .provider = QStringLiteral("claude-code"),
        .windows = {UsageWindow{.label = QStringLiteral("Current session"),
                                .kind = WindowKind::Session,
                                .usedPercent = 23,
                                .isBelowOne = false,
                                .resetsAt = QDateTime(QDate(2026, 10, 4), QTime(11, 50), QTimeZone::UTC)},
                    UsageWindow{.label = QStringLiteral("Daily requests"),
                                .kind = WindowKind::Other,
                                .usedPercent = 0.4,
                                .isBelowOne = true,
                                .resetsAt = std::nullopt}},
        .billingNote = QStringLiteral("You are on a subscription."),
        .insights = Insights{.note = QStringLiteral("Approximate."),
                             .periods = {InsightsPeriod{
                                 .label = QStringLiteral("Last 24h"),
                                 .summary = QStringLiteral("206 requests · 5 sessions"),
                                 .shares = {Share{.label = QStringLiteral("At >150k context"), .percent = 62},
                                            Share{.label = QStringLiteral("unreadable line"), .percent = std::nullopt}},
                                 .rankings = {Ranking{.title = QStringLiteral("Skills"),
                                                      .items = {Share{.label = QStringLiteral("/skill-a"),
                                                                      .percent = 35}}}}}}},
        .rawOutput = QStringLiteral("raw"),
    };
}

} // namespace

class TestReports : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void tokenCountsAddUp() {
        TokenCounts counts{.input = 1, .output = 2, .cacheWrite = 3, .cacheRead = 4};
        QCOMPARE(counts.total(), qint64(10));
        counts += TokenCounts{.input = 10, .output = 0, .cacheWrite = 0, .cacheRead = 1};
        QCOMPARE(counts.input, qint64(11));
        QCOMPARE(counts.total(), qint64(21));
    }

    void limitsReportRoundTrips() {
        const LimitsReport report = sampleLimits();
        const auto decoded = limitsReportFromJson(toJson(report));
        QVERIFY(decoded);
        QVERIFY(*decoded == report);
    }

    void limitsReportWithoutOptionalsRoundTrips() {
        const LimitsReport report{.provider = QStringLiteral("claude-code"),
                                  .windows = {},
                                  .billingNote = std::nullopt,
                                  .insights = std::nullopt,
                                  .rawOutput = std::nullopt};
        const auto decoded = limitsReportFromJson(toJson(report));
        QVERIFY(decoded);
        QVERIFY(*decoded == report);
    }

    void limitsReportRejectsAMissingProvider() {
        QJsonObject json = toJson(sampleLimits());
        json.remove(QStringLiteral("provider"));
        QVERIFY(!limitsReportFromJson(json));
    }

    void limitsReportRejectsAnUnknownKind() {
        QJsonObject json = toJson(sampleLimits());
        QJsonArray windows = json.value(QStringLiteral("windows")).toArray();
        QJsonObject first = windows.at(0).toObject();
        first.insert(QStringLiteral("kind"), QStringLiteral("monthly"));
        windows.replace(0, first);
        json.insert(QStringLiteral("windows"), windows);
        QVERIFY(!limitsReportFromJson(json));
    }

    void activityReportRoundTrips() {
        const ActivityReport report{
            .provider = QStringLiteral("claude-code"),
            .days = {ActivityDay{.day = QDate(2026, 10, 4),
                                 .requests = 668,
                                 .sessions = 8,
                                 .tokens = TokenCounts{.input = 12'000, .output = 812'000, .cacheWrite = 300'000,
                                                       .cacheRead = 4'200'000},
                                 .byModel = {{QStringLiteral("claude-opus-5-5"),
                                              TokenCounts{.input = 12'000, .output = 0, .cacheWrite = 0,
                                                          .cacheRead = 0}}}}},
            .skippedLines = 3,
        };
        const auto decoded = activityReportFromJson(toJson(report));
        QVERIFY(decoded);
        QVERIFY(*decoded == report);
    }

    void activityReportRejectsABadDay() {
        QJsonObject json = toJson(ActivityReport{
            .provider = QStringLiteral("claude-code"),
            .days = {ActivityDay{.day = QDate(2026, 10, 4), .requests = 1, .sessions = 1, .tokens = {}, .byModel = {}}},
            .skippedLines = 0});
        QJsonArray days = json.value(QStringLiteral("days")).toArray();
        QJsonObject day = days.at(0).toObject();
        day.insert(QStringLiteral("day"), QStringLiteral("not a date"));
        days.replace(0, day);
        json.insert(QStringLiteral("days"), days);
        QVERIFY(!activityReportFromJson(json));
    }
};

QTEST_GUILESS_MAIN(TestReports)
#include "tst_reports.moc"
```

Append to `tests/CMakeLists.txt`:
```cmake

ecm_add_test(core/tst_reports.cpp
    TEST_NAME tst_reports
    LINK_LIBRARIES contribusage_core Qt6::Test)
```

- [ ] **Step 2: Run it and confirm it fails**

Run: `cmake -B build -G Ninja && cmake --build build`
Expected: FAIL with `core/reports.h: No such file or directory`.

- [ ] **Step 3: Write the implementation**

`src/core/reports.h`:
```cpp
#pragma once

#include <QDate>
#include <QDateTime>
#include <QJsonObject>
#include <QList>
#include <QMap>
#include <QString>

#include <optional>

namespace contribusage {

// SPEC §10.1: why a source has no current value. Which fields carry data depends on the kind.
struct SourceError {
    enum class Kind {
        ToolNotFound,
        NotLoggedIn,
        UnsupportedPlan, // text: the tool's note (ADR-017)
        TimedOut,
        ProcessFailed, // code: exit code, text: stderr tail
        Unparseable,   // text: raw output
        Offline,
        TokenMissing,
        Unauthorized,
        RateLimited, // until
        Http,        // code: status
        Decoding,    // text: message
        Io,          // text: message
        ProviderSpecific,
    };
    Kind kind;
    QString text = {};
    int code = 0;
    QDateTime until = {};

    bool operator==(const SourceError &) const = default;
};

enum class WindowKind { Session, Weekly, Other };

// SPEC §10.3: one plan limit bucket, its label verbatim (FR-8).
struct UsageWindow {
    QString label;
    WindowKind kind = WindowKind::Other;
    double usedPercent = 0; // can exceed 100; only the UI clamps
    bool isBelowOne = false;
    std::optional<QDateTime> resetsAt;

    bool operator==(const UsageWindow &) const = default;
};

// FR-38: `percent` is empty for a line the provider could not read; `label` then holds the whole line.
struct Share {
    QString label;
    std::optional<int> percent;

    bool operator==(const Share &) const = default;
};

struct Ranking {
    QString title;
    QList<Share> items;

    bool operator==(const Ranking &) const = default;
};

struct InsightsPeriod {
    QString label;
    QString summary;
    QList<Share> shares;
    QList<Ranking> rankings;

    bool operator==(const InsightsPeriod &) const = default;
};

// FR-38, ADR-023: tool supplied, approximate statistics; at least one period.
struct Insights {
    std::optional<QString> note;
    QList<InsightsPeriod> periods;

    bool operator==(const Insights &) const = default;
};

struct LimitsReport {
    QString provider;
    QList<UsageWindow> windows;
    std::optional<QString> billingNote;
    std::optional<Insights> insights;
    std::optional<QString> rawOutput;

    bool operator==(const LimitsReport &) const = default;
};

struct TokenCounts {
    qint64 input = 0;
    qint64 output = 0;
    qint64 cacheWrite = 0;
    qint64 cacheRead = 0;

    qint64 total() const { return input + output + cacheWrite + cacheRead; }
    TokenCounts &operator+=(const TokenCounts &other);
    bool operator==(const TokenCounts &) const = default;
};

// SPEC §10.4: one local calendar day.
struct ActivityDay {
    QDate day;
    qint64 requests = 0;
    qint64 sessions = 0;
    TokenCounts tokens;
    QMap<QString, TokenCounts> byModel;

    bool operator==(const ActivityDay &) const = default;
};

struct ActivityReport {
    QString provider;
    QList<ActivityDay> days; // oldest first, up to 365
    qint64 skippedLines = 0;

    bool operator==(const ActivityReport &) const = default;
};

// The JSON form inside `state.json` and `history.json` (SPEC §10.7). `fromJson` returns nullopt when a required field
// is missing or has the wrong type, so a foreign or damaged file never yields a half-filled report.
QJsonObject toJson(const LimitsReport &report);
std::optional<LimitsReport> limitsReportFromJson(const QJsonObject &json);
QJsonObject toJson(const ActivityReport &report);
std::optional<ActivityReport> activityReportFromJson(const QJsonObject &json);

} // namespace contribusage
```

`src/core/reports.cpp`:
```cpp
#include "core/reports.h"

#include <QJsonArray>

namespace contribusage {

TokenCounts &TokenCounts::operator+=(const TokenCounts &other) {
    input += other.input;
    output += other.output;
    cacheWrite += other.cacheWrite;
    cacheRead += other.cacheRead;
    return *this;
}

namespace {

QJsonValue optionalString(const std::optional<QString> &text) {
    return text ? QJsonValue(*text) : QJsonValue();
}

std::optional<QString> stringOrNull(const QJsonValue &value) {
    return value.isString() ? std::optional<QString>(value.toString()) : std::nullopt;
}

QString kindName(WindowKind kind) {
    switch (kind) {
    case WindowKind::Session:
        return QStringLiteral("session");
    case WindowKind::Weekly:
        return QStringLiteral("weekly");
    case WindowKind::Other:
        break;
    }
    return QStringLiteral("other");
}

std::optional<WindowKind> kindFromName(const QString &name) {
    if (name == u"session") {
        return WindowKind::Session;
    }
    if (name == u"weekly") {
        return WindowKind::Weekly;
    }
    if (name == u"other") {
        return WindowKind::Other;
    }
    return std::nullopt;
}

QJsonValue dateTimeJson(const std::optional<QDateTime> &time) {
    return time ? QJsonValue(time->toUTC().toString(Qt::ISODateWithMs)) : QJsonValue();
}

QJsonArray sharesJson(const QList<Share> &shares) {
    QJsonArray array;
    for (const Share &share : shares) {
        array.append(QJsonObject{{QStringLiteral("label"), share.label},
                                 {QStringLiteral("percent"), share.percent ? QJsonValue(*share.percent) : QJsonValue()}});
    }
    return array;
}

std::optional<QList<Share>> sharesFrom(const QJsonValue &value) {
    if (!value.isArray()) {
        return std::nullopt;
    }
    QList<Share> shares;
    for (const QJsonValue &item : value.toArray()) {
        const QJsonObject object = item.toObject();
        if (!object.value(QStringLiteral("label")).isString()) {
            return std::nullopt;
        }
        const QJsonValue percent = object.value(QStringLiteral("percent"));
        shares.append(Share{.label = object.value(QStringLiteral("label")).toString(),
                            .percent = percent.isDouble() ? std::optional<int>(percent.toInt()) : std::nullopt});
    }
    return shares;
}

QJsonObject tokensJson(const TokenCounts &tokens) {
    return {{QStringLiteral("input"), tokens.input},
            {QStringLiteral("output"), tokens.output},
            {QStringLiteral("cacheWrite"), tokens.cacheWrite},
            {QStringLiteral("cacheRead"), tokens.cacheRead}};
}

TokenCounts tokensFrom(const QJsonObject &object) {
    return {.input = object.value(QStringLiteral("input")).toInteger(),
            .output = object.value(QStringLiteral("output")).toInteger(),
            .cacheWrite = object.value(QStringLiteral("cacheWrite")).toInteger(),
            .cacheRead = object.value(QStringLiteral("cacheRead")).toInteger()};
}

QJsonObject insightsJson(const Insights &insights) {
    QJsonArray periods;
    for (const InsightsPeriod &period : insights.periods) {
        QJsonArray rankings;
        for (const Ranking &ranking : period.rankings) {
            rankings.append(QJsonObject{{QStringLiteral("title"), ranking.title},
                                        {QStringLiteral("items"), sharesJson(ranking.items)}});
        }
        periods.append(QJsonObject{{QStringLiteral("label"), period.label},
                                   {QStringLiteral("summary"), period.summary},
                                   {QStringLiteral("shares"), sharesJson(period.shares)},
                                   {QStringLiteral("rankings"), rankings}});
    }
    return {{QStringLiteral("note"), optionalString(insights.note)}, {QStringLiteral("periods"), periods}};
}

std::optional<Insights> insightsFrom(const QJsonObject &object) {
    if (!object.value(QStringLiteral("periods")).isArray()) {
        return std::nullopt;
    }
    Insights insights{.note = stringOrNull(object.value(QStringLiteral("note"))), .periods = {}};
    for (const QJsonValue &item : object.value(QStringLiteral("periods")).toArray()) {
        const QJsonObject period = item.toObject();
        const auto shares = sharesFrom(period.value(QStringLiteral("shares")));
        if (!period.value(QStringLiteral("label")).isString() || !shares ||
            !period.value(QStringLiteral("rankings")).isArray()) {
            return std::nullopt;
        }
        QList<Ranking> rankings;
        for (const QJsonValue &rankingItem : period.value(QStringLiteral("rankings")).toArray()) {
            const QJsonObject ranking = rankingItem.toObject();
            const auto items = sharesFrom(ranking.value(QStringLiteral("items")));
            if (!ranking.value(QStringLiteral("title")).isString() || !items) {
                return std::nullopt;
            }
            rankings.append(Ranking{.title = ranking.value(QStringLiteral("title")).toString(), .items = *items});
        }
        insights.periods.append(InsightsPeriod{.label = period.value(QStringLiteral("label")).toString(),
                                               .summary = period.value(QStringLiteral("summary")).toString(),
                                               .shares = *shares,
                                               .rankings = rankings});
    }
    return insights;
}

} // namespace

QJsonObject toJson(const LimitsReport &report) {
    QJsonArray windows;
    for (const UsageWindow &window : report.windows) {
        windows.append(QJsonObject{{QStringLiteral("label"), window.label},
                                   {QStringLiteral("kind"), kindName(window.kind)},
                                   {QStringLiteral("usedPercent"), window.usedPercent},
                                   {QStringLiteral("isBelowOne"), window.isBelowOne},
                                   {QStringLiteral("resetsAt"), dateTimeJson(window.resetsAt)}});
    }
    return {{QStringLiteral("provider"), report.provider},
            {QStringLiteral("windows"), windows},
            {QStringLiteral("billingNote"), optionalString(report.billingNote)},
            {QStringLiteral("insights"), report.insights ? QJsonValue(insightsJson(*report.insights)) : QJsonValue()},
            {QStringLiteral("rawOutput"), optionalString(report.rawOutput)}};
}

std::optional<LimitsReport> limitsReportFromJson(const QJsonObject &json) {
    if (!json.value(QStringLiteral("provider")).isString() || !json.value(QStringLiteral("windows")).isArray()) {
        return std::nullopt;
    }
    LimitsReport report{.provider = json.value(QStringLiteral("provider")).toString(),
                        .windows = {},
                        .billingNote = stringOrNull(json.value(QStringLiteral("billingNote"))),
                        .insights = std::nullopt,
                        .rawOutput = stringOrNull(json.value(QStringLiteral("rawOutput")))};
    for (const QJsonValue &item : json.value(QStringLiteral("windows")).toArray()) {
        const QJsonObject window = item.toObject();
        const auto kind = kindFromName(window.value(QStringLiteral("kind")).toString());
        if (!window.value(QStringLiteral("label")).isString() || !kind ||
            !window.value(QStringLiteral("usedPercent")).isDouble()) {
            return std::nullopt;
        }
        std::optional<QDateTime> resetsAt;
        if (window.value(QStringLiteral("resetsAt")).isString()) {
            const QDateTime time =
                QDateTime::fromString(window.value(QStringLiteral("resetsAt")).toString(), Qt::ISODateWithMs);
            if (!time.isValid()) {
                return std::nullopt;
            }
            resetsAt = time;
        }
        report.windows.append(UsageWindow{.label = window.value(QStringLiteral("label")).toString(),
                                          .kind = *kind,
                                          .usedPercent = window.value(QStringLiteral("usedPercent")).toDouble(),
                                          .isBelowOne = window.value(QStringLiteral("isBelowOne")).toBool(),
                                          .resetsAt = resetsAt});
    }
    if (json.value(QStringLiteral("insights")).isObject()) {
        report.insights = insightsFrom(json.value(QStringLiteral("insights")).toObject());
        if (!report.insights) {
            return std::nullopt;
        }
    }
    return report;
}

QJsonObject toJson(const ActivityReport &report) {
    QJsonArray days;
    for (const ActivityDay &day : report.days) {
        QJsonObject byModel;
        for (auto it = day.byModel.cbegin(); it != day.byModel.cend(); ++it) {
            byModel.insert(it.key(), tokensJson(it.value()));
        }
        days.append(QJsonObject{{QStringLiteral("day"), day.day.toString(Qt::ISODate)},
                                {QStringLiteral("requests"), day.requests},
                                {QStringLiteral("sessions"), day.sessions},
                                {QStringLiteral("tokens"), tokensJson(day.tokens)},
                                {QStringLiteral("byModel"), byModel}});
    }
    return {{QStringLiteral("provider"), report.provider},
            {QStringLiteral("days"), days},
            {QStringLiteral("skippedLines"), report.skippedLines}};
}

std::optional<ActivityReport> activityReportFromJson(const QJsonObject &json) {
    if (!json.value(QStringLiteral("provider")).isString() || !json.value(QStringLiteral("days")).isArray()) {
        return std::nullopt;
    }
    ActivityReport report{.provider = json.value(QStringLiteral("provider")).toString(),
                          .days = {},
                          .skippedLines = json.value(QStringLiteral("skippedLines")).toInteger()};
    for (const QJsonValue &item : json.value(QStringLiteral("days")).toArray()) {
        const QJsonObject day = item.toObject();
        const QDate date = QDate::fromString(day.value(QStringLiteral("day")).toString(), Qt::ISODate);
        if (!date.isValid() || !day.value(QStringLiteral("tokens")).isObject()) {
            return std::nullopt;
        }
        QMap<QString, TokenCounts> byModel;
        const QJsonObject models = day.value(QStringLiteral("byModel")).toObject();
        for (auto it = models.constBegin(); it != models.constEnd(); ++it) {
            byModel.insert(it.key(), tokensFrom(it.value().toObject()));
        }
        report.days.append(ActivityDay{.day = date,
                                       .requests = day.value(QStringLiteral("requests")).toInteger(),
                                       .sessions = day.value(QStringLiteral("sessions")).toInteger(),
                                       .tokens = tokensFrom(day.value(QStringLiteral("tokens")).toObject()),
                                       .byModel = byModel});
    }
    return report;
}

} // namespace contribusage
```

In `src/core/CMakeLists.txt`, add `reports.cpp reports.h` to the `add_library` list (alphabetical: after `heatmap.h`).

- [ ] **Step 4: Run the tests and confirm they pass**

Run: `cmake -B build -G Ninja && cmake --build build && ctest --test-dir build --output-on-failure -R tst_reports`
Expected: PASS.

- [ ] **Step 5: Format, full suite, commit**

Run: `clang-format -i src/core/reports.* tests/core/tst_reports.cpp && clang-format --dry-run --Werror $(git ls-files -co --exclude-standard '*.cpp' '*.h') && QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure`
Expected: no format output; all tests pass.

```bash
git add src/core tests
git commit -m "Core: report models and their JSON form

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 2: Snapshots and source states

**Files:**
- Create: `src/core/snapshot.h`, `tests/core/tst_snapshot.cpp`
- Modify: `src/core/CMakeLists.txt`, `tests/CMakeLists.txt`

**Interfaces:**
- Consumes: `SourceError` (Task 1).
- Produces (header `core/snapshot.h`):
  - `template <typename T> struct Snapshot { T value; QDateTime fetchedAt; QString origin; bool isStale(const QDateTime &now, qint64 staleAfterSeconds) const; }`
  - namespace `origins`: `cache()`, `poll()`, `push()`, `activity()`, `github()`, each returning a `QString`
  - `enum class NotConfiguredReason { ProviderDisabled, ToolNotInstalled, UnsupportedPlan, NoLocalData, GithubTokenMissing }`
  - `template <typename T> struct SourceState`, with:
    - a `phase` (`NotConfigured`, `Loading`, `Loaded` or `Failed`), plus `reason`, `note` and `error`
    - factories `notConfigured(reason, note)`, `loading(previous)`, `loaded(snapshot)` and `failed(error, previous)`
    - `std::optional<Snapshot<T>> snapshot() const`

- [ ] **Step 1: Write the failing test**

`tests/core/tst_snapshot.cpp`:
```cpp
#include "core/snapshot.h"

#include <QTest>
#include <QTimeZone>

using namespace contribusage;

namespace {
const Snapshot<int> fetched{.value = 1, .fetchedAt = QDateTime::fromSecsSinceEpoch(0, QTimeZone::UTC),
                            .origin = origins::cache()};
}

class TestSnapshot : public QObject {
    Q_OBJECT

private Q_SLOTS:
    // SPEC §12 "stale after".
    void snapshotIsStaleOnlyPastTheLimit() {
        QVERIFY(!fetched.isStale(QDateTime::fromSecsSinceEpoch(1800, QTimeZone::UTC), 1800));
        QVERIFY(fetched.isStale(QDateTime::fromSecsSinceEpoch(1801, QTimeZone::UTC), 1800));
    }

    // SPEC §11.3: every state but "not configured" and "first load" can show values.
    void sourceStateExposesTheSnapshotItCanShow() {
        QVERIFY(!SourceState<int>::notConfigured(NotConfiguredReason::NoLocalData).snapshot());
        QVERIFY(!SourceState<int>::loading(std::nullopt).snapshot());
        QVERIFY(SourceState<int>::loading(fetched).snapshot() == fetched);
        QVERIFY(SourceState<int>::loaded(fetched).snapshot() == fetched);
        const SourceError offline{.kind = SourceError::Kind::Offline};
        QVERIFY(SourceState<int>::failed(offline, fetched).snapshot() == fetched);
        QVERIFY(!SourceState<int>::failed(offline, std::nullopt).snapshot());
    }

    void sourceStateKeepsItsDetails() {
        const auto unsupported =
            SourceState<int>::notConfigured(NotConfiguredReason::UnsupportedPlan, QStringLiteral("Needs a login"));
        QCOMPARE(unsupported.phase, SourceState<int>::Phase::NotConfigured);
        QCOMPARE(unsupported.reason, NotConfiguredReason::UnsupportedPlan);
        QCOMPARE(unsupported.note, QStringLiteral("Needs a login"));
        const auto failed = SourceState<int>::failed({.kind = SourceError::Kind::TimedOut}, std::nullopt);
        QCOMPARE(failed.phase, SourceState<int>::Phase::Failed);
        QVERIFY(failed.error && failed.error->kind == SourceError::Kind::TimedOut);
    }
};

QTEST_GUILESS_MAIN(TestSnapshot)
#include "tst_snapshot.moc"
```

Append to `tests/CMakeLists.txt`:
```cmake

ecm_add_test(core/tst_snapshot.cpp
    TEST_NAME tst_snapshot
    LINK_LIBRARIES contribusage_core Qt6::Test)
```

- [ ] **Step 2: Run it and confirm it fails**

Run: `cmake -B build -G Ninja && cmake --build build`
Expected: FAIL with `core/snapshot.h: No such file or directory`.

- [ ] **Step 3: Write the implementation**

`src/core/snapshot.h`:
```cpp
#pragma once

#include "core/reports.h"

#include <QDateTime>
#include <QString>

#include <optional>

namespace contribusage {

// Everything the UI shows is a snapshot (SPEC §5, §10.1).
template <typename T> struct Snapshot {
    T value;
    QDateTime fetchedAt;
    QString origin;

    // SPEC §12 "stale after".
    bool isStale(const QDateTime &now, qint64 staleAfterSeconds) const {
        return fetchedAt.secsTo(now) > staleAfterSeconds;
    }
    bool operator==(const Snapshot &) const = default;
};

// An open set of origins; providers add their own.
namespace origins {
inline QString cache() { return QStringLiteral("cache"); }
inline QString poll() { return QStringLiteral("poll"); }
inline QString push() { return QStringLiteral("push"); }
inline QString activity() { return QStringLiteral("activity"); }
inline QString github() { return QStringLiteral("github"); }
} // namespace origins

enum class NotConfiguredReason { ProviderDisabled, ToolNotInstalled, UnsupportedPlan, NoLocalData, GithubTokenMissing };

// SPEC §11.3: a source's state. While loading or failed it keeps the previous snapshot, so the UI can show it dimmed.
template <typename T> struct SourceState {
    enum class Phase { NotConfigured, Loading, Loaded, Failed };

    Phase phase = Phase::Loading;
    NotConfiguredReason reason = NotConfiguredReason::ProviderDisabled;
    QString note;                      // UnsupportedPlan: the tool's own text (ADR-017)
    std::optional<SourceError> error;  // Failed
    std::optional<Snapshot<T>> current; // Loaded: the value; Loading and Failed: the previous one

    static SourceState notConfigured(NotConfiguredReason reason, const QString &note = {}) {
        return {.phase = Phase::NotConfigured, .reason = reason, .note = note, .error = {}, .current = {}};
    }
    static SourceState loading(std::optional<Snapshot<T>> previous) {
        return {.phase = Phase::Loading, .reason = {}, .note = {}, .error = {}, .current = std::move(previous)};
    }
    static SourceState loaded(Snapshot<T> snapshot) {
        return {.phase = Phase::Loaded, .reason = {}, .note = {}, .error = {}, .current = std::move(snapshot)};
    }
    static SourceState failed(SourceError error, std::optional<Snapshot<T>> previous) {
        return {.phase = Phase::Failed, .reason = {}, .note = {}, .error = std::move(error),
                .current = std::move(previous)};
    }

    // The values the UI can show, current or previous.
    std::optional<Snapshot<T>> snapshot() const { return phase == Phase::NotConfigured ? std::nullopt : current; }
};

} // namespace contribusage
```

In `src/core/CMakeLists.txt`, add `snapshot.h` to the `add_library` list (after `reports.h`).

- [ ] **Step 4: Run the tests and confirm they pass**

Run: `cmake -B build -G Ninja && cmake --build build && ctest --test-dir build --output-on-failure -R tst_snapshot`
Expected: PASS.

- [ ] **Step 5: Format, full suite, commit**

Run: `clang-format -i src/core/snapshot.h tests/core/tst_snapshot.cpp && clang-format --dry-run --Werror $(git ls-files -co --exclude-standard '*.cpp' '*.h') && QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure`
Expected: no format output; all tests pass.

```bash
git add src/core tests
git commit -m "Core: snapshots and source states

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 3: The data folder and versioned atomic JSON files

**Files:**
- Create: `src/core/apppaths.h`, `src/core/apppaths.cpp`, `src/core/jsonstore.h`, `src/core/jsonstore.cpp`, `tests/core/tst_jsonstore.cpp`
- Modify: `src/core/CMakeLists.txt`, `tests/CMakeLists.txt`

**Interfaces:**
- Produces:
  - `struct AppPaths { QString root; static AppPaths live(); QString providerFolder(const QString &providerId) const; }` (header `core/apppaths.h`)
  - `struct StoreError { enum class Kind { UnsupportedSchemaVersion, Malformed, Io }; Kind kind; int version; QString message; }` (header `core/jsonstore.h`)
  - namespace `contribusage::jsonstore`:
    - `std::expected<std::optional<QJsonValue>, StoreError> read(const QString &file, int schemaVersion)`
    - `std::expected<void, StoreError> write(const QString &file, int schemaVersion, const QJsonValue &value)`
    - `bool createFolder(const QString &folder)`
    - `bool deleteProviderData(const QString &providerId, const AppPaths &paths)`

- [ ] **Step 1: Write the failing test**

`tests/core/tst_jsonstore.cpp`:
```cpp
#include "core/apppaths.h"
#include "core/jsonstore.h"

#include <QFile>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QTest>

#include <unistd.h>

using namespace contribusage;

namespace {

constexpr int version = 2;

QJsonObject sample(const QString &label) {
    return {{QStringLiteral("label"), label}};
}

QByteArray contents(const QString &file) {
    QFile input(file);
    return input.open(QIODevice::ReadOnly) ? input.readAll() : QByteArray();
}

} // namespace

// SPEC §10.7 and §16.3: atomic writes, schema versions, history never discarded, provider data deletion.
class TestJsonStore : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void roundTripCreatesTheRootOwnerOnly() {
        QTemporaryDir temporary;
        const AppPaths paths{.root = temporary.filePath(QStringLiteral("contribusage"))};
        const QString file = paths.providerFolder(QStringLiteral("fake")) + QStringLiteral("/history.json");

        const auto missing = jsonstore::read(file, version);
        QVERIFY(missing && !missing->has_value());

        QVERIFY(jsonstore::write(file, version, sample(QStringLiteral("a"))));

        const auto read = jsonstore::read(file, version);
        QVERIFY(read && read->has_value());
        QCOMPARE(read->value().toObject(), sample(QStringLiteral("a")));
        QCOMPARE(contents(file), QByteArray(R"({"schemaVersion":2,"value":{"label":"a"}})"));
        const auto permissions = QFile::permissions(paths.root);
        QVERIFY(permissions.testFlags(QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner));
        QVERIFY(!(permissions & (QFile::ReadGroup | QFile::WriteGroup | QFile::ExeGroup | QFile::ReadOther |
                                 QFile::WriteOther | QFile::ExeOther)));
    }

    // An atomic write replaces the file instead of rewriting it: a hard link to the old file keeps the old content.
    void writeReplacesTheFileAtomically() {
        QTemporaryDir temporary;
        const QString file = temporary.filePath(QStringLiteral("state.json"));
        const QString link = temporary.filePath(QStringLiteral("link.json"));
        QVERIFY(jsonstore::write(file, version, sample(QStringLiteral("old"))));
        QCOMPARE(::link(QFile::encodeName(file).constData(), QFile::encodeName(link).constData()), 0);

        QVERIFY(jsonstore::write(file, version, sample(QStringLiteral("new"))));

        QCOMPARE(jsonstore::read(file, version)->value().toObject(), sample(QStringLiteral("new")));
        QCOMPARE(jsonstore::read(link, version)->value().toObject(), sample(QStringLiteral("old")));
    }

    // Another version is an error and the file stays as it is: caches overwrite it, history.json keeps it.
    void versionMismatchKeepsTheFile() {
        QTemporaryDir temporary;
        const QString file = temporary.filePath(QStringLiteral("history.json"));
        const QByteArray newer = R"({"schemaVersion":3,"value":{"label":"future"}})";
        QFile output(file);
        QVERIFY(output.open(QIODevice::WriteOnly));
        output.write(newer);
        output.close();

        const auto read = jsonstore::read(file, version);
        QVERIFY(!read);
        QCOMPARE(read.error().kind, StoreError::Kind::UnsupportedSchemaVersion);
        QCOMPARE(read.error().version, 3);
        QCOMPARE(contents(file), newer);
    }

    void malformedFileIsAnError() {
        QTemporaryDir temporary;
        const QString file = temporary.filePath(QStringLiteral("state.json"));
        QFile output(file);
        QVERIFY(output.open(QIODevice::WriteOnly));
        output.write("{ not json");
        output.close();

        const auto read = jsonstore::read(file, version);
        QVERIFY(!read);
        QCOMPARE(read.error().kind, StoreError::Kind::Malformed);
        QCOMPARE(contents(file), QByteArray("{ not json"));
    }

    void deletingProviderDataRemovesOnlyThatProvidersFolder() {
        QTemporaryDir temporary;
        const AppPaths paths{.root = temporary.filePath(QStringLiteral("contribusage"))};
        const QString history = QStringLiteral("/history.json");
        QVERIFY(jsonstore::write(paths.providerFolder(QStringLiteral("fake")) + history, version, sample({})));
        QVERIFY(jsonstore::write(paths.providerFolder(QStringLiteral("other")) + history, version, sample({})));
        QVERIFY(jsonstore::write(paths.root + QStringLiteral("/state.json"), version, sample({})));

        QVERIFY(jsonstore::deleteProviderData(QStringLiteral("fake"), paths));
        QVERIFY(jsonstore::deleteProviderData(QStringLiteral("fake"), paths)); // already gone is not an error

        QVERIFY(!QFile::exists(paths.providerFolder(QStringLiteral("fake"))));
        QVERIFY(QFile::exists(paths.providerFolder(QStringLiteral("other"))));
        QVERIFY(QFile::exists(paths.root + QStringLiteral("/state.json")));
    }

    // Port design §2: the data folder follows XDG.
    void liveRootFollowsXdgDataHome() {
        QTemporaryDir temporary;
        const QByteArray saved = qgetenv("XDG_DATA_HOME");
        qputenv("XDG_DATA_HOME", QFile::encodeName(temporary.path()));
        const QString root = AppPaths::live().root;
        if (saved.isEmpty()) {
            qunsetenv("XDG_DATA_HOME");
        } else {
            qputenv("XDG_DATA_HOME", saved);
        }
        QCOMPARE(root, temporary.path() + QStringLiteral("/contribusage"));
        QCOMPARE(AppPaths{.root = root}.providerFolder(QStringLiteral("claude-code")),
                 root + QStringLiteral("/providers/claude-code"));
    }
};

QTEST_GUILESS_MAIN(TestJsonStore)
#include "tst_jsonstore.moc"
```

Append to `tests/CMakeLists.txt`:
```cmake

ecm_add_test(core/tst_jsonstore.cpp
    TEST_NAME tst_jsonstore
    LINK_LIBRARIES contribusage_core Qt6::Test)
```

- [ ] **Step 2: Run it and confirm it fails**

Run: `cmake -B build -G Ninja && cmake --build build`
Expected: FAIL with `core/apppaths.h: No such file or directory`.

- [ ] **Step 3: Write the implementation**

`src/core/apppaths.h`:
```cpp
#pragma once

#include <QString>

namespace contribusage {

// Where the widget keeps its data (port design §2, SPEC §10.7). A struct, not a seam: tests pass a temporary root
// (ADR-015).
struct AppPaths {
    QString root;

    // $XDG_DATA_HOME/contribusage, normally ~/.local/share/contribusage.
    static AppPaths live();
    // root/providers/<id>: everything a provider stores; "Delete data for this provider" removes it (US-12).
    QString providerFolder(const QString &providerId) const;
};

} // namespace contribusage
```

`src/core/apppaths.cpp`:
```cpp
#include "core/apppaths.h"

#include <QStandardPaths>

namespace contribusage {

AppPaths AppPaths::live() {
    return {.root = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) +
                    QStringLiteral("/contribusage")};
}

QString AppPaths::providerFolder(const QString &providerId) const {
    return root + QStringLiteral("/providers/") + providerId;
}

} // namespace contribusage
```

`src/core/jsonstore.h`:
```cpp
#pragma once

#include "core/apppaths.h"

#include <QJsonValue>
#include <QString>

#include <expected>
#include <optional>

namespace contribusage {

// Why a data file could not be read or written (SPEC §10.7, ADR-005).
struct StoreError {
    enum class Kind { UnsupportedSchemaVersion, Malformed, Io };
    Kind kind;
    int version = 0; // UnsupportedSchemaVersion: the version the file holds
    QString message = {};

    bool operator==(const StoreError &) const = default;
};

namespace jsonstore {

// The `value` of `{"schemaVersion": n, "value": …}`, or nullopt when the file does not exist. A file of another version
// or a broken file is an error and stays untouched: a cache's owner overwrites it on the next write, history.json's
// owner backs it up and migrates it.
std::expected<std::optional<QJsonValue>, StoreError> read(const QString &file, int schemaVersion);

// Creates missing folders owner-only (0700), then replaces the file atomically.
std::expected<void, StoreError> write(const QString &file, int schemaVersion, const QJsonValue &value);

// Creates `folder` and its missing parents owner-only; an existing folder keeps its mode.
bool createFolder(const QString &folder);

// US-12: removes root/providers/<id> and nothing else; a missing folder is fine.
bool deleteProviderData(const QString &providerId, const AppPaths &paths);

} // namespace jsonstore
} // namespace contribusage
```

`src/core/jsonstore.cpp`:
```cpp
#include "core/jsonstore.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QStringList>

namespace contribusage::jsonstore {

std::expected<std::optional<QJsonValue>, StoreError> read(const QString &file, int schemaVersion) {
    QFile input(file);
    if (!input.exists()) {
        return std::optional<QJsonValue>();
    }
    if (!input.open(QIODevice::ReadOnly)) {
        return std::unexpected(StoreError{.kind = StoreError::Kind::Io, .message = input.errorString()});
    }
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(input.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        return std::unexpected(StoreError{.kind = StoreError::Kind::Malformed, .message = parseError.errorString()});
    }
    const QJsonObject envelope = document.object();
    const int version = envelope.value(QStringLiteral("schemaVersion")).toInt(-1);
    if (version != schemaVersion) {
        return std::unexpected(StoreError{.kind = StoreError::Kind::UnsupportedSchemaVersion, .version = version});
    }
    return std::optional<QJsonValue>(envelope.value(QStringLiteral("value")));
}

std::expected<void, StoreError> write(const QString &file, int schemaVersion, const QJsonValue &value) {
    const QString folder = QFileInfo(file).absolutePath();
    if (!createFolder(folder)) {
        return std::unexpected(StoreError{.kind = StoreError::Kind::Io, .message = QStringLiteral("cannot create ") + folder});
    }
    QSaveFile output(file);
    if (!output.open(QIODevice::WriteOnly)) {
        return std::unexpected(StoreError{.kind = StoreError::Kind::Io, .message = output.errorString()});
    }
    const QJsonObject envelope{{QStringLiteral("schemaVersion"), schemaVersion}, {QStringLiteral("value"), value}};
    output.write(QJsonDocument(envelope).toJson(QJsonDocument::Compact));
    if (!output.commit()) {
        return std::unexpected(StoreError{.kind = StoreError::Kind::Io, .message = output.errorString()});
    }
    return {};
}

bool createFolder(const QString &folder) {
    const QString path = QDir::cleanPath(QFileInfo(folder).absoluteFilePath());
    QStringList missing;
    QString current = path;
    while (!QFileInfo::exists(current)) {
        missing.prepend(current);
        const QString parent = QFileInfo(current).path();
        if (parent == current) {
            break;
        }
        current = parent;
    }
    for (const QString &directory : missing) {
        if (!QDir().mkdir(directory, QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner)) {
            return false;
        }
    }
    return QFileInfo(path).isDir();
}

bool deleteProviderData(const QString &providerId, const AppPaths &paths) {
    return QDir(paths.providerFolder(providerId)).removeRecursively();
}

} // namespace contribusage::jsonstore
```

In `src/core/CMakeLists.txt`, add `apppaths.cpp apppaths.h` (before `display.cpp`) and `jsonstore.cpp jsonstore.h` (after `heatmap.h`).

- [ ] **Step 4: Run the tests and confirm they pass**

Run: `cmake -B build -G Ninja && cmake --build build && ctest --test-dir build --output-on-failure -R tst_jsonstore`
Expected: PASS.

- [ ] **Step 5: Format, full suite, commit**

Run: `clang-format -i src/core/apppaths.* src/core/jsonstore.* tests/core/tst_jsonstore.cpp && clang-format --dry-run --Werror $(git ls-files -co --exclude-standard '*.cpp' '*.h') && QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure`
Expected: no format output; all tests pass.

```bash
git add src/core tests
git commit -m "Core: data folder and versioned atomic JSON files

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 4: The process runner

**Files:**
- Create: `src/core/process.h`, `src/core/process.cpp`, `tests/core/tst_processrunner.cpp`
- Modify: `src/core/CMakeLists.txt`, `tests/CMakeLists.txt`

**Interfaces:**
- Consumes: `SourceError` (Task 1).
- Produces (header `core/process.h`):
  - `struct ProcessRequest { QString executable; QStringList arguments; QString workingDirectory; std::optional<QProcessEnvironment> environment; int timeoutMs = 30'000; }`
  - `struct ProcessResult { int exitCode; QString stdoutText; QString stderrText; }`
  - `using ProcessOutcome = std::expected<ProcessResult, SourceError>;`
  - `using ProcessCallback = std::function<void(ProcessOutcome)>;`
  - `class ProcessRunning { virtual void run(const ProcessRequest &, ProcessCallback done) = 0; }`
  - `class LiveProcessRunner final : public ProcessRunning`

- [ ] **Step 1: Write the failing test**

`tests/core/tst_processrunner.cpp`:
```cpp
#include "core/process.h"

#include <QDir>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QTemporaryDir>
#include <QTest>
#include <QTimer>

using namespace contribusage;

namespace {

ProcessRequest request(const QString &executable, const QStringList &arguments = {},
                       const QString &folder = QDir::tempPath(), int timeoutMs = 10'000) {
    return {.executable = executable,
            .arguments = arguments,
            .workingDirectory = folder,
            .environment = std::nullopt,
            .timeoutMs = timeoutMs};
}

// Runs `request` and spins the event loop until the callback arrives, or `waitMs` passes.
std::optional<ProcessOutcome> runAndWait(const ProcessRequest &request, int waitMs = 15'000) {
    std::optional<ProcessOutcome> outcome;
    QEventLoop loop;
    LiveProcessRunner().run(request, [&](ProcessOutcome result) {
        outcome = std::move(result);
        loop.quit();
    });
    if (!outcome) {
        QTimer::singleShot(waitMs, &loop, &QEventLoop::quit);
        loop.exec();
    }
    return outcome;
}

} // namespace

// SPEC §16.1 process tests: LiveProcessRunner against real system tools (FR-7, NFR-18).
class TestProcessRunner : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void capturesStdoutAndExitCode() {
        const auto echo = runAndWait(request(QStringLiteral("/bin/echo"), {QStringLiteral("hello")}));
        QVERIFY(echo && echo->has_value());
        QVERIFY(echo->value() == (ProcessResult{.exitCode = 0, .stdoutText = QStringLiteral("hello\n"),
                                                .stderrText = QString()}));
        const auto failing = runAndWait(request(QStringLiteral("/usr/bin/false")));
        QVERIFY(failing && failing->has_value());
        QCOMPARE(failing->value().exitCode, 1);
    }

    void capturesStderrAndUsesTheWorkingDirectoryAndEnvironment() {
        QProcessEnvironment environment;
        environment.insert(QStringLiteral("GREETING"), QStringLiteral("hi"));
        ProcessRequest script = request(QStringLiteral("/bin/sh"),
                                        {QStringLiteral("-c"), QStringLiteral("pwd; echo \"$GREETING\" >&2")},
                                        QStringLiteral("/usr"));
        script.environment = environment;
        const auto result = runAndWait(script);
        QVERIFY(result && result->has_value());
        QCOMPARE(result->value().stdoutText, QStringLiteral("/usr\n"));
        QCOMPARE(result->value().stderrText, QStringLiteral("hi\n"));
    }

    // Output larger than the pipe buffer on both streams deadlocks a runner that waits for exit before reading.
    void readsLargeOutputOnBothStreams() {
        const auto result = runAndWait(request(
            QStringLiteral("/bin/sh"),
            {QStringLiteral("-c"), QStringLiteral("head -c 500000 /dev/zero | tr '\\0' a; "
                                                  "head -c 500000 /dev/zero | tr '\\0' b >&2")}));
        QVERIFY(result && result->has_value());
        QCOMPARE(result->value().stdoutText.size(), qsizetype(500'000));
        QCOMPARE(result->value().stderrText.size(), qsizetype(500'000));
    }

    // `cat` waits forever on an inherited or open stdin.
    void stdinIsDevNull() {
        const auto result = runAndWait(request(QStringLiteral("/bin/cat"), {}, QDir::tempPath(), 5'000));
        QVERIFY(result && result->has_value());
        QCOMPARE(result->value().stdoutText, QString());
    }

    void timeoutFailsWithTimedOut() {
        QElapsedTimer clock;
        clock.start();
        const auto result = runAndWait(request(QStringLiteral("/bin/sleep"), {QStringLiteral("10")}, QDir::tempPath(), 200));
        QVERIFY(result && !result->has_value());
        QCOMPARE(result->error().kind, SourceError::Kind::TimedOut);
        QVERIFY(clock.elapsed() < 2'000);
    }

    void killsAProcessThatIgnoresSIGTERM() {
        QElapsedTimer clock;
        clock.start();
        const auto result = runAndWait(request(QStringLiteral("/bin/sh"),
                                               {QStringLiteral("-c"), QStringLiteral("trap '' TERM; exec /bin/sleep 10")},
                                               QDir::tempPath(), 200));
        QVERIFY(result && !result->has_value());
        QCOMPARE(result->error().kind, SourceError::Kind::TimedOut);
        QVERIFY(clock.elapsed() < 5'000); // 0.2 s timeout + 2 s grace, not 10 s
    }

    void missingExecutableFailsAndTheQueueMovesOn() {
        const auto missing = runAndWait(request(QStringLiteral("/no/such/tool")));
        QVERIFY(missing && !missing->has_value());
        QCOMPARE(missing->error().kind, SourceError::Kind::Io);
        const auto next = runAndWait(request(QStringLiteral("/bin/echo"), {QStringLiteral("next")}));
        QVERIFY(next && next->has_value());
        QCOMPARE(next->value().stdoutText, QStringLiteral("next\n"));
    }

    // NFR-18: `mkdir` is atomic, so a second process running at the same time fails to take the lock.
    void runsOneProcessAtATime() {
        QTemporaryDir folder;
        const ProcessRequest script =
            request(QStringLiteral("/bin/sh"),
                    {QStringLiteral("-c"), QStringLiteral("mkdir lock || exit 7; sleep 0.2; rmdir lock")}, folder.path());
        QList<int> codes;
        QEventLoop loop;
        LiveProcessRunner runner;
        for (int index = 0; index < 3; ++index) {
            runner.run(script, [&](ProcessOutcome outcome) {
                codes.append(outcome ? outcome->exitCode : -1);
                if (codes.size() == 3) {
                    loop.quit();
                }
            });
        }
        QTimer::singleShot(15'000, &loop, &QEventLoop::quit);
        loop.exec();
        QCOMPARE(codes, (QList<int>{0, 0, 0}));
    }
};

QTEST_GUILESS_MAIN(TestProcessRunner)
#include "tst_processrunner.moc"
```

Append to `tests/CMakeLists.txt`:
```cmake

ecm_add_test(core/tst_processrunner.cpp
    TEST_NAME tst_processrunner
    LINK_LIBRARIES contribusage_core Qt6::Test)
```

- [ ] **Step 2: Run it and confirm it fails**

Run: `cmake -B build -G Ninja && cmake --build build`
Expected: FAIL with `core/process.h: No such file or directory`.

- [ ] **Step 3: Write the implementation**

`src/core/process.h`:
```cpp
#pragma once

#include "core/reports.h"

#include <QProcessEnvironment>
#include <QString>
#include <QStringList>

#include <expected>
#include <functional>
#include <optional>

namespace contribusage {

struct ProcessRequest {
    QString executable;
    QStringList arguments;
    QString workingDirectory;
    std::optional<QProcessEnvironment> environment; // nullopt inherits the widget's environment
    int timeoutMs = 30'000;
};

struct ProcessResult {
    int exitCode = 0;
    QString stdoutText;
    QString stderrText;

    bool operator==(const ProcessResult &) const = default;
};

using ProcessOutcome = std::expected<ProcessResult, SourceError>;
using ProcessCallback = std::function<void(ProcessOutcome)>;

// SPEC §10.6: runs a child process without blocking the caller. `done` is called exactly once, on the caller's thread,
// with the result, `TimedOut`, or `Io` when the executable cannot start.
class ProcessRunning {
public:
    virtual ~ProcessRunning() = default;
    virtual void run(const ProcessRequest &request, ProcessCallback done) = 0;
};

// FR-7, NFR-18: stdin /dev/null, both streams captured, the timeout sends SIGTERM and SIGKILL 2 s later. At most one
// process runs at a time across all instances; later requests wait their turn.
class LiveProcessRunner final : public ProcessRunning {
public:
    void run(const ProcessRequest &request, ProcessCallback done) override;
};

} // namespace contribusage
```

`src/core/process.cpp`:
```cpp
#include "core/process.h"

#include <QList>
#include <QObject>
#include <QProcess>
#include <QTimer>

#include <memory>
#include <utility>

namespace contribusage {

namespace {

struct Job {
    ProcessRequest request;
    ProcessCallback done;
};

// NFR-18: one child process at a time, first come first served.
class ProcessQueue : public QObject {
public:
    static ProcessQueue &instance() {
        static ProcessQueue queue;
        return queue;
    }

    void enqueue(Job job) {
        m_jobs.append(std::move(job));
        if (!m_busy) {
            startNext();
        }
    }

private:
    void startNext();

    QList<Job> m_jobs;
    bool m_busy = false;
};

void ProcessQueue::startNext() {
    if (m_jobs.isEmpty()) {
        m_busy = false;
        return;
    }
    m_busy = true;
    Job job = m_jobs.takeFirst();

    auto *process = new QProcess(this);
    process->setProgram(job.request.executable);
    process->setArguments(job.request.arguments);
    process->setWorkingDirectory(job.request.workingDirectory);
    if (job.request.environment) {
        process->setProcessEnvironment(*job.request.environment);
    }
    process->setStandardInputFile(QProcess::nullDevice());

    auto *deadline = new QTimer(process);
    deadline->setSingleShot(true);
    auto timedOut = std::make_shared<bool>(false);
    auto done = std::make_shared<ProcessCallback>(std::move(job.done));

    // Reports once, frees the process, then starts the next job.
    auto complete = [this, process, deadline, done](ProcessOutcome outcome) {
        if (!*done) {
            return;
        }
        deadline->stop();
        const ProcessCallback callback = std::exchange(*done, nullptr);
        process->deleteLater();
        callback(std::move(outcome));
        startNext();
    };

    connect(deadline, &QTimer::timeout, process, [process, timedOut] {
        *timedOut = true;
        process->terminate();
        QTimer::singleShot(2'000, process, [process] {
            if (process->state() != QProcess::NotRunning) {
                process->kill();
            }
        });
    });
    connect(process, &QProcess::errorOccurred, this, [process, complete](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart) {
            complete(std::unexpected(SourceError{.kind = SourceError::Kind::Io, .text = process->errorString()}));
        }
    });
    connect(process, &QProcess::finished, this, [process, timedOut, complete](int exitCode, QProcess::ExitStatus) {
        if (*timedOut) {
            complete(std::unexpected(SourceError{.kind = SourceError::Kind::TimedOut}));
            return;
        }
        complete(ProcessResult{.exitCode = exitCode,
                               .stdoutText = QString::fromUtf8(process->readAllStandardOutput()),
                               .stderrText = QString::fromUtf8(process->readAllStandardError())});
    });

    // The deadline starts first: a launch failure can report before start() returns.
    deadline->start(job.request.timeoutMs);
    process->start();
}

} // namespace

void LiveProcessRunner::run(const ProcessRequest &request, ProcessCallback done) {
    ProcessQueue::instance().enqueue(Job{.request = request, .done = std::move(done)});
}

} // namespace contribusage
```

In `src/core/CMakeLists.txt`, add `process.cpp process.h` (after `jsonstore.h`).

- [ ] **Step 4: Run the tests and confirm they pass**

Run: `cmake -B build -G Ninja && cmake --build build && ctest --test-dir build --output-on-failure -R tst_processrunner`
Expected: PASS within about 5 s.

- [ ] **Step 5: Format, full suite, commit**

Run: `clang-format -i src/core/process.* tests/core/tst_processrunner.cpp && clang-format --dry-run --Werror $(git ls-files -co --exclude-standard '*.cpp' '*.h') && QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure`
Expected: no format output; all tests pass.

```bash
git add src/core tests
git commit -m "Core: non-blocking process runner, one child at a time

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 5: Contract and vault

**Files:**
- Modify: `SPEC.md` (§17.8), `llm-wiki/modules/core.md`, `llm-wiki/log.md`

- [ ] **Step 1: Split L-1 in the contract and tick L-1a**

In `SPEC.md` §17.8, replace the line starting `- [ ] **L-1**` with:
```markdown
- [x] **L-1a** Core models, persistence and the process runner in C++: the report types with their JSON form, snapshots and source states, the XDG data folder with versioned atomic JSON files, and a non-blocking `QProcess` runner that runs one child at a time; Swift tests ported from `2603a21`. Accept: `tst_reports`, `tst_snapshot`, `tst_jsonstore` and `tst_processrunner` pass.
- [ ] **L-1b** File events (inotify), the file reading seam with the declared-roots check, the secret store (KWallet), the JSONL reader and the history store.
- [ ] **L-1c** Schedule, refresh coordinator, notification planner, popup layout and menu bar label.
```

- [ ] **Step 2: Update the vault**

`llm-wiki/modules/core.md`:
- set `status: active` and `updated: 2026-10-04`
- replace the line starting `> In C++ so far:` with:
```markdown
> In C++ so far: `display.h` and `heatmap.h` (L-4a); `reports.h` (SPEC §10.1, §10.3, §10.4 types and their JSON form), `snapshot.h` (snapshots, source states), `apppaths.h` and `jsonstore.h` (SPEC §10.7 in the XDG data folder) and `process.h` (the `ProcessRunning` seam and `LiveProcessRunner`, FR-7, NFR-18) from L-1a. Errors are values (`std::expected`), as KDE builds without exceptions. The process runner has no cancellation yet; it comes with the refresh coordinator (L-1c). `jsonstore::read` reads directly until the file reading seam lands (L-1b).
```

`llm-wiki/log.md`, append:
```markdown

## [2026-10-04] feat | Core models, persistence and process runner (L-1a)
- `reports.h`, `snapshot.h`, `apppaths.h`, `jsonstore.h` and `process.h` in `contribusage_core`, with the Swift tests ported to Qt Test.
- L-1 is split into L-1a, L-1b and L-1c in SPEC §17.8.
```

- [ ] **Step 3: Run every check and commit**

Run: `llm-wiki/tools/wiki.sh lint && cmake --build build && QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure && clang-format --dry-run --Werror $(git ls-files '*.cpp' '*.h') && echo all-ok`
Expected: `wiki lint: clean`, all tests pass, then `all-ok`.

```bash
git add SPEC.md llm-wiki
git commit -m "Spec: L-1 split; L-1a done

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```
