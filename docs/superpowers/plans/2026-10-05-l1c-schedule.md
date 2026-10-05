# L-1c: Schedule, Refresh, Notification Planning and the Panel Label — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace the fixed timers of the limits and GitHub jobs with SPEC §12's refresh policy (intervals with bounds, backoff, manual refresh with a floor, extra triggers on popup open and reset times, no runs while asleep or offline, a 10 s wake delay), wire the Refresh button, port the notification planner and the menu bar label logic into the core.

**Architecture:**
- **Core:** `schedule.h` (the pure `nextRun` of SPEC §12 rule 7 with `SchedulePolicy` and `ScheduleConditions`), `sourceschedule.h` (a QObject per polled source that arms one timer from `nextRun` and emits `due()`; `ScheduleConditionsSource` shares the machine's conditions), `notificationplanner.h` (FR-13 to FR-15, pure) and `menubarlabel.h` (FR-12 modes and SPEC §11.1 label, pure).
- **Jobs:** `LimitsJob` and `GitHubJob` take a `SchedulePolicy` and run when their `SourceSchedule` says so; `refresh()` becomes the manual refresh; `popupOpened()` adds the popup trigger.
- **Platform:** `SystemConditions` feeds `ScheduleConditionsSource` from logind's `PrepareForSleep` (Qt6::DBus) and `QNetworkInformation` (Qt6::Network).
- **Applet:** Refresh button and popup-open trigger; the panel label through `MenuBarLabel`.
- **Rulings taken in the plan:** one schedule per job instead of one coordinator object (ADR-045); notification delivery (KNotification), the planner's persistence, `PopoverLayout` and the label mode setting arrive with the Settings pages in L-5, which consume them; Low Power Mode has no Plasma signal wired yet and stays `false`.

**Tech Stack:** C++23, Qt 6 Core, DBus, Network, QML, Qt Test.

**Spec:**
- **Requirements:** SPEC §12 (refresh policy, rules 1 to 8), FR-11, FR-12, FR-13 to FR-15, §11.1 (label), §13 (recoveries: unsupported plan re-check every 6 h).
- **Decisions:** ADR-018 (coordinator shape), ADR-019 (popup trigger dropped when not due), ADR-020, ADR-022, ADR-027, ADR-038, ADR-039.
- **Swift reference** (`git show 2603a21:<path>`): `Packages/ContribusageKit/Sources/ContribusageCore/{Scheduling/Schedule,Notifications/NotificationPlanner,Models/MenuBarLabel}.swift` and their tests in `Packages/ContribusageKit/Tests/ContribusageCoreTests/`.

## Global Constraints

- C++23, `-Werror`, clang-format clean (only `*.cpp`/`*.h`), `QStringLiteral`, wrap `QStringBuilder` results in `QString(...)` before `QVariant`, no exceptions; `src/core/` links only Qt6::Core.
- Policies (SPEC §12): Claude Code limits 5 min / 1 min / 60 min, stale 30 min, manual floor 30 s, needs the network; GitHub 30 min / 10 min / 6 h, stale 2 h, manual floor 30 s, needs the network. Backoff 2×, 4×, 8×, capped at the maximum. Wake delay 10 s.
- A rate-limited GitHub source never runs before the reset, not even manually (ADR-020). No token, 401 or refused wallet: no automatic runs until the token changes; a manual refresh still runs (ADR-022).

## Review Focus

1. **The Refresh button pressed many times.** Expected: at most one run per source per 30 s floor, never two at once. Pinned in Task 2 (`manualRespectsTheFloor`) and Task 3 (`manualRefreshRunsOnceWithinTheFloor`).
2. **Network gone.** Expected: no probe and no GitHub request while offline, a run as soon as it is back. Pinned in Task 2 (`offlineWaitsForTheNetwork`).
3. **Suspend and resume.** Expected: nothing runs while asleep, sources that are due run 10 s after the wake. Pinned in Task 1 (`nothingRunsWhileAsleepAndWakeTriggersAfterTenSeconds`) and Task 4 (`sleepSignalSetsAsleepThenWake`).
4. **A window's reset time passes while the popup is closed.** Expected: a run at the reset time (once the minimum interval has passed). Pinned in Task 3 (`aResetTimeTriggersARun`).
5. **Repeated failures.** Expected: the interval doubles up to 8× and returns to normal after one success. Pinned in Task 2 (`backoffDoublesAndResetsOnSuccess`).

---

## File Structure

| Path | Responsibility |
|---|---|
| `src/core/schedule.h/.cpp` | `SchedulePolicy`, `ScheduleConditions`, `nextRun` |
| `src/core/sourceschedule.h/.cpp` | `ScheduleConditionsSource`, `SourceSchedule` |
| `src/core/notificationplanner.h/.cpp` | `NotificationCycle`, `NotificationNote`, `NotificationSettings`, `planNotifications` |
| `src/core/menubarlabel.h/.cpp` | `MenuBarMode`, `MenuBarLabel`, `menuBarLabel` |
| `src/claudecode/limitsjob.h/.cpp`, `src/github/githubjob.h/.cpp` | jobs on a `SourceSchedule` |
| `src/claudecode/limitsview.cpp` | `labelView` over `menuBarLabel` |
| `src/platform/systemconditions.h/.cpp` | logind and QNetworkInformation into `ScheduleConditionsSource` |
| `src/applet/popupmodel.*`, `Footer.qml`, `main.qml` | Refresh, popup trigger, conditions |

---

### Task 1: The scheduling decision

**Files:**
- Create: `src/core/schedule.h`, `src/core/schedule.cpp`, `tests/core/tst_schedule.cpp`
- Modify: `src/core/CMakeLists.txt`, `tests/CMakeLists.txt`

**Interfaces:**
- Produces (namespace `contribusage`, header `core/schedule.h`):
  - `struct SchedulePolicy { qint64 defaultMs, minimumMs, maximumMs, staleAfterMs, manualFloorMs; bool needsNetwork; static SchedulePolicy every(qint64 ms); }`
  - `struct ScheduleConditions { bool online = true; bool lowPower = false; bool asleep = false; std::optional<QDateTime> lastWake; }`
  - `inline constexpr qint64 wakeDelayMs = 10'000;`
  - `std::optional<QDateTime> nextRun(const SchedulePolicy &, const std::optional<QDateTime> &lastSuccess, const std::optional<QDateTime> &lastAttempt, int failures, const QDateTime &now, const ScheduleConditions &, bool manual = false, const QList<QDateTime> &triggers = {})`

- [ ] **Step 1: Write the failing test**

`tests/core/tst_schedule.cpp`:
```cpp
#include "core/schedule.h"

#include <QTest>

using namespace contribusage;

namespace {

const QDateTime now(QDate(2026, 5, 9), QTime(12, 0), QTimeZone::UTC);
constexpr qint64 minute = 60'000;

// A 15 min probe policy (the Claude Code row before ADR-039), so interval, backoff and floor stay apart.
const SchedulePolicy probe{.defaultMs = 15 * minute,
                           .minimumMs = 5 * minute,
                           .maximumMs = 60 * minute,
                           .staleAfterMs = 30 * minute,
                           .manualFloorMs = 30'000,
                           .needsNetwork = true};

std::optional<QDateTime> next(const SchedulePolicy &policy = probe, std::optional<QDateTime> lastAttempt = now,
                              int failures = 0, const ScheduleConditions &conditions = {}, bool manual = false,
                              const QList<QDateTime> &triggers = {}) {
    return nextRun(policy, std::nullopt, lastAttempt, failures, now, conditions, manual, triggers);
}

QDateTime at(qint64 ms) {
    return now.addMSecs(ms);
}

} // namespace

// SPEC §12 rule 7, ported from ScheduleTests.swift.
class TestSchedule : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void aSourceThatNeverRanIsDueNow() { QCOMPARE(next(probe, std::nullopt), std::optional(now)); }

    void theDefaultIntervalFollowsTheLastRun() {
        QCOMPARE(next(), std::optional(at(15 * minute)));
        QCOMPARE(nextRun(probe, at(-20 * minute), at(-30 * minute), 0, now, {}), std::optional(at(-5 * minute)));
    }

    // NFR-5: an interval below the minimum is raised to it, one above the maximum lowered.
    void theIntervalStaysWithinMinimumAndMaximum() {
        SchedulePolicy tooShort = probe;
        tooShort.defaultMs = minute;
        SchedulePolicy tooLong = probe;
        tooLong.defaultMs = 120 * minute;
        QCOMPARE(next(tooShort), std::optional(at(5 * minute)));
        QCOMPARE(next(tooLong), std::optional(at(60 * minute)));
    }

    void backoffGrowsAndIsCapped_data() {
        QTest::addColumn<int>("failures");
        QTest::addColumn<qint64>("minutes");
        QTest::newRow("1") << 1 << qint64(30);
        QTest::newRow("2") << 2 << qint64(60);
        QTest::newRow("3") << 3 << qint64(60);
        QTest::newRow("9") << 9 << qint64(60);
    }
    void backoffGrowsAndIsCapped() {
        QFETCH(int, failures);
        QFETCH(qint64, minutes);
        QCOMPARE(next(probe, now, failures), std::optional(at(minutes * minute)));
    }

    void backoffStopsDoublingAfterEightTimes() {
        const SchedulePolicy wide{.defaultMs = minute,
                                  .minimumMs = minute,
                                  .maximumMs = 1440 * minute,
                                  .staleAfterMs = 30 * minute,
                                  .manualFloorMs = 30'000,
                                  .needsNetwork = false};
        QCOMPARE(next(wide, now, 3), std::optional(at(8 * minute)));
        QCOMPARE(next(wide, now, 4), std::optional(at(8 * minute)));
    }

    void lowPowerDoublesTheInterval() {
        QCOMPARE(next(probe, now, 0, {.online = true, .lowPower = true, .asleep = false, .lastWake = std::nullopt}),
                 std::optional(at(30 * minute)));
    }

    void offlineSkipsOnlySourcesThatNeedTheNetwork() {
        const ScheduleConditions offline{.online = false, .lowPower = false, .asleep = false, .lastWake = std::nullopt};
        SchedulePolicy local = probe;
        local.needsNetwork = false;
        QCOMPARE(next(probe, now, 0, offline), std::nullopt);
        QCOMPARE(next(probe, now, 0, offline, true), std::nullopt);
        QCOMPARE(next(local, now, 0, offline), std::optional(at(15 * minute)));
    }

    // SPEC §12 rule 1 and the wake trigger: 10 s after waking a source runs once its minimum interval has passed; a run
    // after the wake consumes it.
    void nothingRunsWhileAsleepAndWakeTriggersAfterTenSeconds() {
        const ScheduleConditions asleep{.online = true, .lowPower = false, .asleep = true, .lastWake = std::nullopt};
        QCOMPARE(next(probe, std::nullopt, 0, asleep), std::nullopt);
        const ScheduleConditions woken{.online = true, .lowPower = false, .asleep = false, .lastWake = now};
        QCOMPARE(next(probe, at(-60 * minute), 0, woken), std::optional(at(10'000)));
        QCOMPARE(next(probe, at(-10 * minute), 0, woken), std::optional(at(10'000)));
        QCOMPARE(next(probe, at(-2 * minute), 0, woken), std::optional(at(3 * minute)));
        const ScheduleConditions earlier{.online = true, .lowPower = false, .asleep = false, .lastWake = at(-minute)};
        QCOMPARE(next(probe, now, 0, earlier), std::optional(at(15 * minute)));
    }

    // Manual refresh ignores interval and backoff but not the floor.
    void manualRefreshRespectsOnlyTheFloor() {
        QCOMPARE(next(probe, at(-10'000), 3, {}, true), std::optional(at(20'000)));
        QCOMPARE(next(probe, at(-20 * minute), 0, {}, true), std::optional(at(-20 * minute + 30'000)));
    }

    // Extra triggers skip interval and backoff but wait out the minimum interval, never delay a run the interval brings
    // sooner, and count only after the last run.
    void aTriggerWaitsOnlyForTheMinimumInterval() {
        QCOMPARE(next(probe, at(-10 * minute), 2, {}, false, {at(minute)}), std::optional(at(minute)));
        QCOMPARE(next(probe, at(-2 * minute), 0, {}, false, {now}), std::optional(at(3 * minute)));
        QCOMPARE(next(probe, at(-14 * minute), 0, {}, false, {at(5 * minute)}), std::optional(at(minute)));
        const ScheduleConditions asleep{.online = true, .lowPower = false, .asleep = true, .lastWake = std::nullopt};
        QCOMPARE(next(probe, at(-10 * minute), 0, asleep, false, {now}), std::nullopt);
        QCOMPARE(next(probe, at(-10 * minute), 0, {}, false, {at(-11 * minute), at(2 * minute)}),
                 std::optional(at(2 * minute)));
    }
};

QTEST_GUILESS_MAIN(TestSchedule)
#include "tst_schedule.moc"
```

Append to `tests/CMakeLists.txt`:
```cmake

ecm_add_test(core/tst_schedule.cpp
    TEST_NAME tst_schedule
    LINK_LIBRARIES contribusage_core Qt6::Test)
```

- [ ] **Step 2: Run it and confirm it fails**

Run: `cmake -B build -G Ninja && cmake --build build`
Expected: FAIL with `core/schedule.h: No such file or directory`.

- [ ] **Step 3: Write the decision**

`src/core/schedule.h`:
```cpp
#pragma once

#include <QDateTime>
#include <QList>

#include <optional>

namespace contribusage {

// SPEC §10.2: how often a polled source runs. Milliseconds, so tests can run real schedules in a few ms.
struct SchedulePolicy {
    qint64 defaultMs = 0;
    qint64 minimumMs = 0;
    qint64 maximumMs = 0;
    qint64 staleAfterMs = 0;
    qint64 manualFloorMs = 0;
    bool needsNetwork = false; // skipped while offline (SPEC §12 rule 2)

    // A fixed interval without a floor or network: for tests and local sources.
    static SchedulePolicy every(qint64 ms) {
        return {.defaultMs = ms,
                .minimumMs = ms,
                .maximumMs = ms,
                .staleAfterMs = 6 * ms,
                .manualFloorMs = 0,
                .needsNetwork = false};
    }
    bool operator==(const SchedulePolicy &) const = default;
};

// What the machine is doing (SPEC §12 rules 1 to 3), fed by the platform layer.
struct ScheduleConditions {
    bool online = true;
    bool lowPower = false;
    bool asleep = false;
    std::optional<QDateTime> lastWake;

    bool operator==(const ScheduleConditions &) const = default;
};

inline constexpr qint64 wakeDelayMs = 10'000;

// SPEC §12 rule 7: when a polled source runs next, possibly in the past (it is due); nullopt while asleep or while
// offline for a source that needs the network. `manual` replaces interval and backoff by the manual floor. `triggers`
// (popup opened, a window's reset), joined by the wake plus 10 s: the first one after the last run runs the source, but
// not within the minimum interval (rule 8).
std::optional<QDateTime> nextRun(const SchedulePolicy &policy, const std::optional<QDateTime> &lastSuccess,
                                 const std::optional<QDateTime> &lastAttempt, int failures, const QDateTime &now,
                                 const ScheduleConditions &conditions, bool manual = false,
                                 const QList<QDateTime> &triggers = {});

} // namespace contribusage
```

`src/core/schedule.cpp`:
```cpp
#include "core/schedule.h"

#include <algorithm>

namespace contribusage {

namespace {

// Clamped to the bounds; 2×, 4×, 8× after failures up to the maximum; doubled in low power mode.
qint64 interval(const SchedulePolicy &policy, int failures, bool lowPower) {
    qint64 value = std::min(std::max(policy.defaultMs, policy.minimumMs), policy.maximumMs);
    value = std::min(value * (qint64(1) << std::min(failures, 3)), policy.maximumMs);
    return lowPower ? value * 2 : value;
}

} // namespace

std::optional<QDateTime> nextRun(const SchedulePolicy &policy, const std::optional<QDateTime> &lastSuccess,
                                 const std::optional<QDateTime> &lastAttempt, int failures, const QDateTime &now,
                                 const ScheduleConditions &conditions, bool manual, const QList<QDateTime> &triggers) {
    if (conditions.asleep || (policy.needsNetwork && !conditions.online)) {
        return std::nullopt;
    }
    std::optional<QDateTime> wake;
    if (conditions.lastWake) {
        wake = conditions.lastWake->addMSecs(wakeDelayMs);
    }
    std::optional<QDateTime> last = lastSuccess;
    if (lastAttempt && (!last || *lastAttempt > *last)) {
        last = lastAttempt;
    }
    QDateTime next = now;
    if (last) {
        next = last->addMSecs(manual ? policy.manualFloorMs : interval(policy, failures, conditions.lowPower));
        QList<QDateTime> candidates = triggers;
        if (wake) {
            candidates.append(*wake);
        }
        std::optional<QDateTime> first;
        for (const QDateTime &trigger : std::as_const(candidates)) {
            if (trigger > *last && (!first || trigger < *first)) {
                first = trigger;
            }
        }
        if (first) {
            next = std::min(next, std::max(*first, last->addMSecs(policy.minimumMs)));
        }
    }
    if (wake) {
        next = std::max(next, *wake);
    }
    return next;
}

} // namespace contribusage
```

In `src/core/CMakeLists.txt`, add `schedule.cpp schedule.h` after `reports.h`.

- [ ] **Step 4: Run the tests and confirm they pass**

Run: `cmake --build build && ctest --test-dir build --output-on-failure -R tst_schedule`
Expected: PASS.

- [ ] **Step 5: Format, full suite, commit**

Run: `clang-format -i src/core/schedule.* tests/core/tst_schedule.cpp && clang-format --dry-run --Werror $(git ls-files -co --exclude-standard '*.cpp' '*.h') && QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure`
Expected: no format output; all tests pass.

```bash
git add src/core tests
git commit -m "Core: the scheduling decision of SPEC §12

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 2: A schedule per source

**Files:**
- Create: `src/core/sourceschedule.h`, `src/core/sourceschedule.cpp`, `tests/core/tst_sourceschedule.cpp`
- Modify: `src/core/CMakeLists.txt`, `tests/CMakeLists.txt`

**Interfaces:**
- Consumes: `nextRun`, `SchedulePolicy`, `ScheduleConditions` (Task 1).
- Produces (header `core/sourceschedule.h`):
  - `class ScheduleConditionsSource : public QObject { const ScheduleConditions &conditions() const; void setConditions(const ScheduleConditions &); signal changed(); }`
  - `class SourceSchedule : public QObject { SourceSchedule(SchedulePolicy, ScheduleConditionsSource *conditions = nullptr, QObject *parent = nullptr); const SchedulePolicy &policy() const; void start(); void finished(bool success); void trigger(const QDateTime &at); bool triggerIfDue(); void requestManual(); void holdUntil(const QDateTime &until); void setPaused(bool); void reset(); std::optional<QDateTime> lastRun() const; signal due(); }`

- [ ] **Step 1: Write the failing test**

`tests/core/tst_sourceschedule.cpp`:
```cpp
#include "core/sourceschedule.h"

#include <QSignalSpy>
#include <QTest>

using namespace contribusage;

namespace {

SchedulePolicy policy(qint64 interval, qint64 minimum, qint64 maximum, qint64 floor, bool network = false) {
    return {.defaultMs = interval,
            .minimumMs = minimum,
            .maximumMs = maximum,
            .staleAfterMs = 6 * interval,
            .manualFloorMs = floor,
            .needsNetwork = network};
}

} // namespace

// SPEC §12: one polled source's runs.
class TestSourceSchedule : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void aSourceThatNeverRanRunsAtOnce() {
        SourceSchedule schedule(SchedulePolicy::every(60'000));
        QSignalSpy due(&schedule, &SourceSchedule::due);
        schedule.start();
        QTRY_COMPARE(due.size(), 1);
    }

    void waitsTheIntervalAfterASuccess() {
        SourceSchedule schedule(SchedulePolicy::every(300));
        QSignalSpy due(&schedule, &SourceSchedule::due);
        schedule.start();
        QTRY_COMPARE(due.size(), 1);
        schedule.finished(true);
        QTest::qWait(150);
        QCOMPARE(due.size(), 1);
        QTRY_COMPARE(due.size(), 2);
    }

    // Review Focus 5: 2× after a failure, back to the interval after a success.
    void backoffDoublesAndResetsOnSuccess() {
        SourceSchedule schedule(policy(200, 200, 2'000, 0));
        QSignalSpy due(&schedule, &SourceSchedule::due);
        schedule.start();
        QTRY_COMPARE(due.size(), 1);
        schedule.finished(false); // next in 400 ms
        QTest::qWait(300);
        QCOMPARE(due.size(), 1);
        QTRY_COMPARE(due.size(), 2);
        schedule.finished(true); // next in 200 ms
        QElapsedTimer timer;
        timer.start();
        QTRY_COMPARE(due.size(), 3);
        QVERIFY(timer.elapsed() < 350);
    }

    // Review Focus 1: a manual refresh skips the interval but not the floor, and never starts a second run.
    void manualRespectsTheFloor() {
        SourceSchedule schedule(policy(60'000, 60'000, 60'000, 300));
        QSignalSpy due(&schedule, &SourceSchedule::due);
        schedule.start();
        QTRY_COMPARE(due.size(), 1);
        schedule.requestManual(); // still running: nothing
        QTest::qWait(50);
        QCOMPARE(due.size(), 1);
        schedule.finished(true);
        schedule.requestManual(); // the floor: 300 ms after the last run
        QTest::qWait(150);
        QCOMPARE(due.size(), 1);
        QTRY_COMPARE(due.size(), 2);
    }

    // ADR-022: paused, only a manual refresh runs.
    void pausedRunsOnlyManually() {
        SourceSchedule schedule(policy(100, 100, 100, 0));
        QSignalSpy due(&schedule, &SourceSchedule::due);
        schedule.start();
        QTRY_COMPARE(due.size(), 1);
        schedule.setPaused(true);
        schedule.finished(false);
        QTest::qWait(300);
        QCOMPARE(due.size(), 1);
        schedule.requestManual();
        QTRY_COMPARE(due.size(), 2);
    }

    // ADR-020: nothing before the hold, not even manually.
    void holdBlocksEvenManualRuns() {
        SourceSchedule schedule(policy(60'000, 60'000, 60'000, 0));
        QSignalSpy due(&schedule, &SourceSchedule::due);
        schedule.start();
        QTRY_COMPARE(due.size(), 1);
        schedule.holdUntil(QDateTime::currentDateTimeUtc().addMSecs(300));
        schedule.finished(false);
        schedule.requestManual();
        QTest::qWait(150);
        QCOMPARE(due.size(), 1);
        QTRY_COMPARE(due.size(), 2);
    }

    // Review Focus 2: offline, a source that needs the network waits; it runs when the network is back.
    void offlineWaitsForTheNetwork() {
        ScheduleConditionsSource conditions;
        conditions.setConditions({.online = false, .lowPower = false, .asleep = false, .lastWake = std::nullopt});
        SourceSchedule schedule(policy(60'000, 60'000, 60'000, 0, true), &conditions);
        QSignalSpy due(&schedule, &SourceSchedule::due);
        schedule.start();
        QTest::qWait(100);
        QCOMPARE(due.size(), 0);
        conditions.setConditions({});
        QTRY_COMPARE(due.size(), 1);
    }

    // ADR-019: the popup trigger counts only when the minimum interval has passed; otherwise it is dropped.
    void popupTriggerRespectsTheMinimum() {
        SourceSchedule schedule(policy(60'000, 200, 60'000, 0));
        QSignalSpy due(&schedule, &SourceSchedule::due);
        schedule.start();
        QTRY_COMPARE(due.size(), 1);
        schedule.finished(true);
        QVERIFY(!schedule.triggerIfDue());
        QTest::qWait(300);
        QCOMPARE(due.size(), 1); // the dropped trigger did not come back
        QVERIFY(schedule.triggerIfDue());
        QTRY_COMPARE(due.size(), 2);
    }

    void aFutureTriggerRunsAtItsTime() {
        SourceSchedule schedule(policy(60'000, 100, 60'000, 0));
        QSignalSpy due(&schedule, &SourceSchedule::due);
        schedule.start();
        QTRY_COMPARE(due.size(), 1);
        schedule.finished(true);
        schedule.trigger(QDateTime::currentDateTimeUtc().addMSecs(300));
        QTest::qWait(150);
        QCOMPARE(due.size(), 1);
        QTRY_COMPARE(due.size(), 2);
    }

    // A new account: the old runs, failures, holds and pauses no longer count.
    void resetMakesItDueNow() {
        SourceSchedule schedule(SchedulePolicy::every(60'000));
        QSignalSpy due(&schedule, &SourceSchedule::due);
        schedule.start();
        QTRY_COMPARE(due.size(), 1);
        schedule.setPaused(true);
        schedule.reset(); // also ends the run in flight: its result belongs to the old account
        QTRY_COMPARE(due.size(), 2);
    }
};

QTEST_GUILESS_MAIN(TestSourceSchedule)
#include "tst_sourceschedule.moc"
```
(also add `#include <QElapsedTimer>` at the top.)

Append to `tests/CMakeLists.txt`:
```cmake

ecm_add_test(core/tst_sourceschedule.cpp
    TEST_NAME tst_sourceschedule
    LINK_LIBRARIES contribusage_core Qt6::Test)
```

- [ ] **Step 2: Run it and confirm it fails**

Run: `cmake -B build -G Ninja && cmake --build build`
Expected: FAIL with `core/sourceschedule.h: No such file or directory`.

- [ ] **Step 3: Write the schedule**

`src/core/sourceschedule.h`:
```cpp
#pragma once

#include "core/schedule.h"

#include <QObject>
#include <QPointer>
#include <QTimer>

namespace contribusage {

// The machine's conditions, shared by every source's schedule; the platform layer sets them (SPEC §12 rules 1 to 3).
class ScheduleConditionsSource : public QObject {
    Q_OBJECT

public:
    using QObject::QObject;

    const ScheduleConditions &conditions() const { return m_conditions; }
    void setConditions(const ScheduleConditions &conditions);

Q_SIGNALS:
    void changed();

private:
    ScheduleConditions m_conditions;
};

// One polled source's runs (SPEC §12): arms a single timer for nextRun() and emits `due()` when it fires. Between `due()`
// and `finished()` the source is running and nothing else starts it. Without a conditions source the machine counts as
// awake and online.
class SourceSchedule : public QObject {
    Q_OBJECT

public:
    explicit SourceSchedule(SchedulePolicy policy, ScheduleConditionsSource *conditions = nullptr,
                            QObject *parent = nullptr);

    const SchedulePolicy &policy() const { return m_policy; }
    // The later of the last success and the last attempt.
    std::optional<QDateTime> lastRun() const;

    void start();
    // The run that `due()` started ended; a failure counts toward the backoff.
    void finished(bool success);
    // An extra trigger at `at` (a window's reset time, rule 8).
    void trigger(const QDateTime &at);
    // The popup opened: a trigger now if the minimum interval has passed since the last run, else dropped (ADR-019).
    bool triggerIfDue();
    // Runs as soon as the manual floor allows, ignoring interval and backoff (rule 4).
    void requestManual();
    // No run before `until`, not even a manual one (ADR-020); cleared by a success.
    void holdUntil(const QDateTime &until);
    // No automatic runs; a manual refresh still runs (ADR-022).
    void setPaused(bool paused);
    // A new account: forgets runs, failures, triggers, hold and pause, ends the run in flight (its result is the old
    // account's and the source drops it) and runs now.
    void reset();

Q_SIGNALS:
    void due();

private:
    void arm();
    void fire();

    SchedulePolicy m_policy;
    QPointer<ScheduleConditionsSource> m_conditions;
    QTimer m_timer;
    std::optional<QDateTime> m_lastSuccess;
    std::optional<QDateTime> m_lastAttempt;
    std::optional<QDateTime> m_hold;
    QList<QDateTime> m_triggers;
    int m_failures = 0;
    bool m_started = false;
    bool m_running = false;
    bool m_manual = false;
    bool m_paused = false;
};

} // namespace contribusage
```

`src/core/sourceschedule.cpp`:
```cpp
#include "core/sourceschedule.h"

#include <algorithm>
#include <limits>

namespace contribusage {

void ScheduleConditionsSource::setConditions(const ScheduleConditions &conditions) {
    if (conditions == m_conditions) {
        return;
    }
    m_conditions = conditions;
    Q_EMIT changed();
}

SourceSchedule::SourceSchedule(SchedulePolicy policy, ScheduleConditionsSource *conditions, QObject *parent)
    : QObject(parent), m_policy(policy), m_conditions(conditions) {
    m_timer.setSingleShot(true);
    connect(&m_timer, &QTimer::timeout, this, &SourceSchedule::fire);
    if (conditions) {
        connect(conditions, &ScheduleConditionsSource::changed, this, &SourceSchedule::arm);
    }
}

std::optional<QDateTime> SourceSchedule::lastRun() const {
    if (m_lastAttempt && (!m_lastSuccess || *m_lastAttempt > *m_lastSuccess)) {
        return m_lastAttempt;
    }
    return m_lastSuccess;
}

void SourceSchedule::start() {
    m_started = true;
    arm();
}

void SourceSchedule::finished(bool success) {
    m_running = false;
    const QDateTime now = QDateTime::currentDateTimeUtc();
    if (success) {
        m_lastSuccess = now;
        m_failures = 0;
        m_hold.reset();
    } else {
        ++m_failures;
    }
    const std::optional<QDateTime> last = lastRun();
    m_triggers.removeIf([&last](const QDateTime &trigger) { return last && trigger <= *last; });
    arm();
}

void SourceSchedule::trigger(const QDateTime &at) {
    m_triggers.append(at);
    arm();
}

bool SourceSchedule::triggerIfDue() {
    const QDateTime now = QDateTime::currentDateTimeUtc();
    const std::optional<QDateTime> last = lastRun();
    if (m_running || (last && last->msecsTo(now) < m_policy.minimumMs)) {
        return false;
    }
    trigger(now);
    return true;
}

void SourceSchedule::requestManual() {
    m_manual = true;
    arm();
}

void SourceSchedule::holdUntil(const QDateTime &until) {
    m_hold = until;
    arm();
}

void SourceSchedule::setPaused(bool paused) {
    m_paused = paused;
    arm();
}

void SourceSchedule::reset() {
    m_lastSuccess.reset();
    m_lastAttempt.reset();
    m_hold.reset();
    m_triggers.clear();
    m_failures = 0;
    m_running = false;
    m_manual = false;
    m_paused = false;
    arm();
}

void SourceSchedule::arm() {
    m_timer.stop();
    if (!m_started || m_running || (m_paused && !m_manual)) {
        return;
    }
    const QDateTime now = QDateTime::currentDateTimeUtc();
    const ScheduleConditions conditions = m_conditions ? m_conditions->conditions() : ScheduleConditions{};
    std::optional<QDateTime> next =
        nextRun(m_policy, m_lastSuccess, m_lastAttempt, m_failures, now, conditions, m_manual, m_triggers);
    if (!next) {
        return; // asleep or offline: a change of the conditions arms again
    }
    if (m_hold && *m_hold > *next) {
        next = m_hold;
    }
    const qint64 delay = std::clamp<qint64>(now.msecsTo(*next), 0, std::numeric_limits<int>::max());
    m_timer.start(int(delay));
}

void SourceSchedule::fire() {
    m_running = true;
    m_manual = false;
    m_lastAttempt = QDateTime::currentDateTimeUtc();
    Q_EMIT due();
}

} // namespace contribusage
```

In `src/core/CMakeLists.txt`, add `sourceschedule.cpp sourceschedule.h` after `snapshot.h`.

- [ ] **Step 4: Run the tests and confirm they pass**

Run: `cmake --build build && ctest --test-dir build --output-on-failure -R tst_sourceschedule --repeat until-fail:3`
Expected: PASS three times.

- [ ] **Step 5: Format, full suite, commit**

Run: `clang-format -i src/core/sourceschedule.* tests/core/tst_sourceschedule.cpp && clang-format --dry-run --Werror $(git ls-files -co --exclude-standard '*.cpp' '*.h') && QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure`
Expected: no format output; all tests pass.

```bash
git add src/core tests
git commit -m "Core: a schedule per polled source

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 3: The jobs on their schedules

**Files:**
- Modify: `src/claudecode/limitsjob.h`, `src/claudecode/limitsjob.cpp`, `src/github/githubjob.h`, `src/github/githubjob.cpp`, `tests/claudecode/tst_limitsjob.cpp`, `tests/github/tst_githubjob.cpp`

**Interfaces:**
- Consumes: `SourceSchedule`, `ScheduleConditionsSource`, `SchedulePolicy` (Tasks 1, 2).
- Produces:
  - `LimitsJob(ClaudeCodeLimits &, AppPaths, SchedulePolicy, ScheduleConditionsSource *conditions = nullptr, QObject *parent = nullptr)`; `void refresh()` is the manual refresh; `void popupOpened()`; `static SchedulePolicy defaultPolicy()` (SPEC §12 Claude Code row).
  - `GitHubJob(GitHubAccount &, AppPaths, SchedulePolicy, ScheduleConditionsSource *conditions = nullptr, QObject *parent = nullptr)`; `void refresh()` is the manual refresh (runs also when paused, never before a rate limit's reset); `void popupOpened()`; `static SchedulePolicy defaultPolicy()` (SPEC §12 GitHub row).

- [ ] **Step 1: Update the tests to the new constructors and add the failing ones**

In `tests/claudecode/tst_limitsjob.cpp` and `tests/github/tst_githubjob.cpp`, add `#include "core/sourceschedule.h"` and replace every interval argument of a job constructor: `60'000` → `SchedulePolicy::every(60'000)`, `100` → `SchedulePolicy::every(100)`.

In `tests/github/tst_githubjob.cpp`, replace the body of `unauthorizedPausesUntilTheTokenChanges` with (a manual refresh now runs while paused, ADR-022, so the pause is shown by the intervals passing):
```cpp
        Fixture fixture;
        fixture.secrets.values.insert(QStringLiteral("token"), QStringLiteral("ghp_old"));
        fixture.transport.respond(401, "{}");
        GitHubJob job(fixture.account, fixture.paths(), SchedulePolicy::every(100));
        job.start();
        QTRY_COMPARE(job.state().phase, Phase::Failed);
        QCOMPARE(job.state().error->kind, SourceError::Kind::Unauthorized);
        const qsizetype sent = fixture.transport.requests.size();
        QTest::qWait(400); // several intervals pass
        QCOMPARE(fixture.transport.requests.size(), sent);
        fixture.transport.respond(200, ::fixture("calendar"));
        job.tokenChanged();
        QTRY_COMPARE(job.state().phase, Phase::Loaded);
```

Add to `TestGitHubJob`:
```cpp
    // ADR-022: a manual refresh runs while paused; ADR-020: never before the rate limit's reset.
    void manualRefreshRunsWhilePausedButNotBeforeTheReset() {
        Fixture fixture;
        fixture.secrets.values.insert(QStringLiteral("token"), QStringLiteral("ghp_x"));
        fixture.transport.respond(401, "{}");
        GitHubJob job(fixture.account, fixture.paths(), SchedulePolicy::every(60'000));
        job.start();
        QTRY_COMPARE(job.state().phase, Phase::Failed);
        fixture.transport.respond(200, ::fixture("calendar"));
        job.refresh();
        QTRY_COMPARE(job.state().phase, Phase::Loaded);
        const qint64 reset = QDateTime::currentSecsSinceEpoch() + 3600;
        fixture.transport.respond(403, "{}",
                                  {{"x-ratelimit-remaining", "0"}, {"x-ratelimit-reset", QByteArray::number(reset)}});
        job.refresh();
        QTRY_COMPARE(job.state().phase, Phase::Failed);
        const qsizetype sent = fixture.transport.requests.size();
        job.refresh();
        QTest::qWait(100);
        QCOMPARE(fixture.transport.requests.size(), sent);
    }
```

Add to `TestLimitsJob`:
```cpp
    // Review Focus 1: pressing Refresh again within the floor runs once.
    void manualRefreshRunsOnceWithinTheFloor() {
        Fixture fixture;
        fixture.runner.answer(claude, QStringLiteral("-p"),
                              FakeProcessRunner::exited(0, ::fixture("subscription-basic")));
        SchedulePolicy policy = SchedulePolicy::every(60'000);
        policy.manualFloorMs = 60'000;
        LimitsJob job(fixture.limits, fixture.paths(), policy);
        job.start();
        QTRY_COMPARE(job.state().phase, Phase::Loaded);
        const qsizetype probes = fixture.probes();
        job.refresh();
        job.refresh();
        QTest::qWait(100);
        QCOMPARE(fixture.probes(), probes);
    }

    // Review Focus 4: a window's reset time is a trigger.
    void aResetTimeTriggersARun() {
        Fixture fixture;
        fixture.runner.answer(claude, QStringLiteral("-p"),
                              FakeProcessRunner::exited(0, ::fixture("subscription-basic")));
        SchedulePolicy policy = SchedulePolicy::every(3'600'000);
        policy.minimumMs = 0;
        LimitsJob job(fixture.limits, fixture.paths(), policy);
        job.start();
        QTRY_COMPARE(job.state().phase, Phase::Loaded);
        const qsizetype probes = fixture.probes();
        job.addResetTrigger(QDateTime::currentDateTimeUtc().addMSecs(200));
        QTRY_VERIFY(fixture.probes() > probes);
    }

    void policiesFollowSpec() {
        const SchedulePolicy limits = LimitsJob::defaultPolicy();
        QCOMPARE(limits.defaultMs, qint64(5 * 60'000));
        QCOMPARE(limits.minimumMs, qint64(60'000));
        QCOMPARE(limits.maximumMs, qint64(60 * 60'000));
        QCOMPARE(limits.manualFloorMs, qint64(30'000));
        QVERIFY(limits.needsNetwork);
    }
```

- [ ] **Step 2: Run them and confirm they fail**

Run: `cmake --build build`
Expected: FAIL to compile (`no matching function for call to 'LimitsJob::LimitsJob(..., SchedulePolicy)'`).

- [ ] **Step 3: Move the jobs onto their schedules**

`src/claudecode/limitsjob.h` — replace the class with:
```cpp
// The limits source (ADR-039) with FR-10's cache and SPEC §11.3's states, run by its SourceSchedule (SPEC §12).
class LimitsJob : public QObject {
    Q_OBJECT

public:
    LimitsJob(ClaudeCodeLimits &limits, AppPaths paths, SchedulePolicy policy,
              ScheduleConditionsSource *conditions = nullptr, QObject *parent = nullptr);

    // SPEC §12, Claude Code row: 5 min, 1 min to 60 min, stale after 30 min, manual floor 30 s, needs the network.
    static SchedulePolicy defaultPolicy();

    const SourceState<LimitsReport> &state() const { return m_state; }

    // Shows the cached snapshot and runs the schedule.
    void start();
    // The manual refresh (SPEC §12 rule 4).
    void refresh();
    // SPEC §12: the popup opened; runs when the data is older than 5 min and the minimum interval has passed.
    void popupOpened();
    // A window's reset time (rule 8); the job adds its windows' times after each success.
    void addResetTrigger(const QDateTime &at);

Q_SIGNALS:
    void stateChanged();

private:
    void run();
    void finish(LimitsOutcome outcome);

    ClaudeCodeLimits &m_limits;
    AppPaths m_paths;
    SourceSchedule m_schedule;
    SourceState<LimitsReport> m_state = SourceState<LimitsReport>::loading(std::nullopt);
};
```
and replace `#include <QTimer>` with `#include "core/sourceschedule.h"`.

`src/claudecode/limitsjob.cpp` — replace everything from the constructor to the end of `finish` with:
```cpp
LimitsJob::LimitsJob(ClaudeCodeLimits &limits, AppPaths paths, SchedulePolicy policy,
                     ScheduleConditionsSource *conditions, QObject *parent)
    : QObject(parent), m_limits(limits), m_paths(std::move(paths)), m_schedule(policy, conditions) {
    connect(&m_schedule, &SourceSchedule::due, this, &LimitsJob::run);
}

SchedulePolicy LimitsJob::defaultPolicy() {
    return {.defaultMs = 5 * 60'000,
            .minimumMs = 60'000,
            .maximumMs = 60 * 60'000,
            .staleAfterMs = 30 * 60'000,
            .manualFloorMs = 30'000,
            .needsNetwork = true};
}

void LimitsJob::start() {
    m_state = SourceState<LimitsReport>::loading(limitscache::load(m_paths, providerId()));
    Q_EMIT stateChanged();
    m_schedule.start();
}

void LimitsJob::refresh() {
    m_schedule.requestManual();
}

void LimitsJob::popupOpened() {
    const auto snapshot = m_state.snapshot();
    if (!snapshot || snapshot->fetchedAt.msecsTo(QDateTime::currentDateTimeUtc()) > 5 * 60'000) {
        m_schedule.triggerIfDue();
    }
}

void LimitsJob::addResetTrigger(const QDateTime &at) {
    m_schedule.trigger(at);
}

void LimitsJob::run() {
    // `this` as context: a job deleted during a probe never gets its result (L-1a's guard).
    m_limits.fetch(this, [this](LimitsOutcome outcome) { finish(std::move(outcome)); });
}

void LimitsJob::finish(LimitsOutcome outcome) {
    const QDateTime now = QDateTime::currentDateTimeUtc();
    m_schedule.finished(outcome.has_value());
    if (outcome) {
        const Snapshot<LimitsReport> snapshot{.value = std::move(*outcome), .fetchedAt = now, .origin = origins::poll()};
        limitscache::save(m_paths, snapshot);
        m_state = SourceState<LimitsReport>::loaded(snapshot);
        for (const UsageWindow &window : snapshot.value.windows) {
            if (window.resetsAt && *window.resetsAt > now) {
                m_schedule.trigger(*window.resetsAt); // FR-11
            }
        }
    } else if (outcome.error().kind == SourceError::Kind::ToolNotFound) {
        m_state = SourceState<LimitsReport>::notConfigured(NotConfiguredReason::ToolNotInstalled);
    } else if (outcome.error().kind == SourceError::Kind::UnsupportedPlan) {
        m_state = SourceState<LimitsReport>::notConfigured(NotConfiguredReason::UnsupportedPlan, outcome.error().text);
        m_schedule.holdUntil(now.addSecs(6 * 3600)); // SPEC §13: re-check every 6 h (ADR-018)
    } else {
        m_state = SourceState<LimitsReport>::failed(outcome.error(), m_state.snapshot());
    }
    Q_EMIT stateChanged();
}
```

`src/github/githubjob.h` — in the class:
- replace the constructor with `GitHubJob(GitHubAccount &account, AppPaths paths, SchedulePolicy policy, ScheduleConditionsSource *conditions = nullptr, QObject *parent = nullptr);`
- add `static SchedulePolicy defaultPolicy(); // SPEC §12 GitHub row: 30 min, 10 min to 6 h, stale 2 h, floor 30 s, network`
- replace the `refresh()` declaration with:
```cpp
    // The manual refresh: runs also while paused (ADR-022), never before a rate limit's reset (ADR-020).
    void refresh();
    // SPEC §12: the popup opened; runs when the data is older than 10 min and the minimum interval has passed.
    void popupOpened();
```
- add a private `void run();`
- replace the members `int m_interval; QTimer m_timer; bool m_fetching = false; bool m_paused = false; ... QDateTime m_waitUntil;` with:
```cpp
    SourceSchedule m_schedule; // paused after a 401, a missing token or a refused wallet; held by a rate limit
    int m_generation = 0;      // bumped by tokenChanged(); a fetch from an older one finishes into nothing
```
- replace `#include <QTimer>` (if present) by `#include "core/sourceschedule.h"`.

`src/github/githubjob.cpp` — replace everything from the constructor through `tokenChanged()` with:
```cpp
GitHubJob::GitHubJob(GitHubAccount &account, AppPaths paths, SchedulePolicy policy,
                     ScheduleConditionsSource *conditions, QObject *parent)
    : QObject(parent), m_account(account), m_paths(std::move(paths)), m_schedule(policy, conditions) {
    connect(&m_schedule, &SourceSchedule::due, this, &GitHubJob::run);
}

SchedulePolicy GitHubJob::defaultPolicy() {
    return {.defaultMs = 30 * 60'000,
            .minimumMs = 10 * 60'000,
            .maximumMs = 6 * 3600'000,
            .staleAfterMs = 2 * 3600'000,
            .manualFloorMs = 30'000,
            .needsNetwork = true};
}

void GitHubJob::start() {
    m_state = SourceState<GitHubReport>::loading(githubcache::load(m_paths));
    Q_EMIT stateChanged();
    m_schedule.start();
}

void GitHubJob::refresh() {
    m_schedule.requestManual();
}

void GitHubJob::popupOpened() {
    const auto snapshot = m_state.snapshot();
    if (!snapshot || snapshot->fetchedAt.msecsTo(QDateTime::currentDateTimeUtc()) > 10 * 60'000) {
        m_schedule.triggerIfDue();
    }
}

void GitHubJob::run() {
    m_account.report(QDateTime::currentDateTime(), QLocale().firstDayOfWeek(), this,
                     [this, generation = m_generation](std::expected<GitHubReport, SourceError> outcome) {
                         if (generation == m_generation) {
                             finish(std::move(outcome));
                         }
                     });
}

void GitHubJob::tokenChanged() {
    ++m_generation;
    githubcache::clear(m_paths);
    m_state = SourceState<GitHubReport>::loading(std::nullopt);
    Q_EMIT stateChanged();
    m_schedule.reset();
}
```
and in `finish`, replace `m_fetching = false;` with `m_schedule.finished(outcome.has_value());`, replace `m_paused = true;` (both places) with `m_schedule.setPaused(true);`, replace `m_waitUntil = outcome.error().until; // ADR-020` with `m_schedule.holdUntil(outcome.error().until.value_or(QDateTime::currentDateTimeUtc())); // ADR-020`, and after the success branch's `m_state = ...loaded(snapshot);` add `m_schedule.setPaused(false);`.

(If `SourceError::until` is a plain `QDateTime` rather than an optional, write `m_schedule.holdUntil(outcome.error().until);`.)

- [ ] **Step 4: Run the tests and confirm they pass**

Run: `cmake --build build && QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure -R "tst_limitsjob|tst_githubjob"`
Expected: PASS. The applet does not compile yet if it still passes intervals; Task 6 changes it — if the full build fails only in `src/applet/popupmodel.cpp`, change its two job constructions now to `LimitsJob::defaultPolicy()` and `github::GitHubJob::defaultPolicy()` (dropping the old interval arguments) so the build stays green.

- [ ] **Step 5: Format, full suite, commit**

Run: `clang-format -i src/claudecode/limitsjob.* src/github/githubjob.* src/applet/popupmodel.cpp tests/claudecode/tst_limitsjob.cpp tests/github/tst_githubjob.cpp && clang-format --dry-run --Werror $(git ls-files -co --exclude-standard '*.cpp' '*.h') && QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure`
Expected: no format output; all tests pass.

```bash
git add src tests
git commit -m "Limits and GitHub jobs run on their SPEC §12 schedules

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 4: The machine's conditions

**Files:**
- Create: `src/platform/systemconditions.h`, `src/platform/systemconditions.cpp`, `tests/platform/tst_systemconditions.cpp`
- Modify: `CMakeLists.txt` (root: Qt6 `DBus` component), `src/platform/CMakeLists.txt`, `tests/CMakeLists.txt`

**Interfaces:**
- Consumes: `ScheduleConditionsSource`, `ScheduleConditions` (Tasks 1, 2).
- Produces (header `platform/systemconditions.h`):
  - `ScheduleConditions afterSleepSignal(ScheduleConditions, bool sleeping, const QDateTime &now)` (pure)
  - `class SystemConditions : public QObject { explicit SystemConditions(ScheduleConditionsSource &target, QObject *parent = nullptr); void onPrepareForSleep(bool sleeping); }`

- [ ] **Step 1: Write the failing test**

`tests/platform/tst_systemconditions.cpp`:
```cpp
#include "platform/systemconditions.h"

#include <QTest>

using namespace contribusage;

// SPEC §12 rules 1 and 2 on Linux: logind's PrepareForSleep and QNetworkInformation.
class TestSystemConditions : public QObject {
    Q_OBJECT

private Q_SLOTS:
    // Review Focus 3: asleep while suspending; on resume, awake with the wake time (the schedule adds 10 s).
    void sleepSignalSetsAsleepThenWake() {
        const QDateTime now(QDate(2026, 10, 5), QTime(12, 0), QTimeZone::UTC);
        const ScheduleConditions asleep = afterSleepSignal({}, true, now);
        QVERIFY(asleep.asleep);
        QCOMPARE(asleep.lastWake, std::nullopt);
        const ScheduleConditions woken = afterSleepSignal(asleep, false, now.addSecs(600));
        QVERIFY(!woken.asleep);
        QCOMPARE(woken.lastWake, std::optional(now.addSecs(600)));
        QVERIFY(woken.online);
    }

    void theSignalUpdatesTheSharedConditions() {
        ScheduleConditionsSource target;
        SystemConditions system(target);
        system.onPrepareForSleep(true);
        QVERIFY(target.conditions().asleep);
        system.onPrepareForSleep(false);
        QVERIFY(!target.conditions().asleep);
        QVERIFY(target.conditions().lastWake.has_value());
    }
};

QTEST_GUILESS_MAIN(TestSystemConditions)
#include "tst_systemconditions.moc"
```

Append to `tests/CMakeLists.txt`:
```cmake

ecm_add_test(platform/tst_systemconditions.cpp
    TEST_NAME tst_systemconditions
    LINK_LIBRARIES contribusage_platform Qt6::Test)
```

- [ ] **Step 2: Run it and confirm it fails**

Run: `cmake -B build -G Ninja && cmake --build build`
Expected: FAIL with `platform/systemconditions.h: No such file or directory`.

- [ ] **Step 3: Write the conditions**

`src/platform/systemconditions.h`:
```cpp
#pragma once

#include "core/sourceschedule.h"

#include <QObject>

namespace contribusage {

// SPEC §12 rule 1: logind's PrepareForSleep(true) means asleep, (false) means awake again at `now`.
ScheduleConditions afterSleepSignal(ScheduleConditions conditions, bool sleeping, const QDateTime &now);

// Feeds the shared schedule conditions from the system: suspend and resume from logind (D-Bus, system bus) and the
// network's reachability from QNetworkInformation. Without either service the conditions stay "awake and online".
class SystemConditions : public QObject {
    Q_OBJECT

public:
    explicit SystemConditions(ScheduleConditionsSource &target, QObject *parent = nullptr);

public Q_SLOTS:
    void onPrepareForSleep(bool sleeping);

private:
    void onReachability();

    ScheduleConditionsSource &m_target;
};

} // namespace contribusage
```

`src/platform/systemconditions.cpp`:
```cpp
#include "platform/systemconditions.h"

#include <QDBusConnection>
#include <QNetworkInformation>

namespace contribusage {

ScheduleConditions afterSleepSignal(ScheduleConditions conditions, bool sleeping, const QDateTime &now) {
    conditions.asleep = sleeping;
    if (!sleeping) {
        conditions.lastWake = now;
    }
    return conditions;
}

SystemConditions::SystemConditions(ScheduleConditionsSource &target, QObject *parent)
    : QObject(parent), m_target(target) {
    QDBusConnection::systemBus().connect(QStringLiteral("org.freedesktop.login1"),
                                         QStringLiteral("/org/freedesktop/login1"),
                                         QStringLiteral("org.freedesktop.login1.Manager"),
                                         QStringLiteral("PrepareForSleep"), this, SLOT(onPrepareForSleep(bool)));
    if (QNetworkInformation::loadBackendByFeatures(QNetworkInformation::Feature::Reachability)) {
        connect(QNetworkInformation::instance(), &QNetworkInformation::reachabilityChanged, this,
                &SystemConditions::onReachability);
        onReachability();
    }
}

void SystemConditions::onPrepareForSleep(bool sleeping) {
    m_target.setConditions(afterSleepSignal(m_target.conditions(), sleeping, QDateTime::currentDateTimeUtc()));
}

void SystemConditions::onReachability() {
    ScheduleConditions conditions = m_target.conditions();
    // Unknown counts as online: a backend that cannot tell must not stop every source.
    conditions.online = QNetworkInformation::instance()->reachability() != QNetworkInformation::Reachability::Disconnected;
    m_target.setConditions(conditions);
}

} // namespace contribusage
```

In the root `CMakeLists.txt`, add `DBus` to the Qt6 components: `find_package(Qt6 ${QT_MIN_VERSION} REQUIRED COMPONENTS Core DBus Gui Network Qml Quick Test)`.

In `src/platform/CMakeLists.txt`, add `systemconditions.cpp systemconditions.h` to the sources and `Qt6::DBus` to its `target_link_libraries`.

- [ ] **Step 4: Run the tests and confirm they pass**

Run: `cmake -B build -G Ninja && cmake --build build && QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure -R tst_systemconditions`
Expected: PASS.

- [ ] **Step 5: Format, full suite, commit**

Run: `clang-format -i src/platform/systemconditions.* tests/platform/tst_systemconditions.cpp && clang-format --dry-run --Werror $(git ls-files -co --exclude-standard '*.cpp' '*.h') && QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure`
Expected: no format output; all tests pass.

```bash
git add CMakeLists.txt src/platform tests
git commit -m "Platform: suspend, resume and reachability as schedule conditions

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 5: Notification planner and menu bar label

**Files:**
- Create: `src/core/notificationplanner.h`, `src/core/notificationplanner.cpp`, `src/core/menubarlabel.h`, `src/core/menubarlabel.cpp`, `tests/core/tst_notificationplanner.cpp`, `tests/core/tst_menubarlabel.cpp`
- Modify: `src/core/CMakeLists.txt`, `tests/CMakeLists.txt`

**Interfaces:**
- Consumes: `LimitsReport`, `UsageWindow`, `Snapshot`, `percentText` (`core/display.h`).
- Produces:
  - `core/notificationplanner.h`: `struct NotificationCycle { std::optional<QDateTime> resetsAt; int threshold; QDateTime sentAt; }`, `using NotificationsSent = QMap<QString, QMap<QString, NotificationCycle>>;`, `struct NotificationNote { QString id; QString provider; QString title; }`, `struct NotificationSettings { QList<int> thresholds{80, 95}; bool notifyOnReset = false; }`, `QList<NotificationNote> planNotifications(const LimitsReport &, const QString &displayName, const std::function<QString(const QString &)> &windowTitle, const NotificationSettings &, NotificationsSent &sent, const QDateTime &now)`
  - `core/menubarlabel.h`: `enum class MenuBarMode { Primary, Weekly, Highest, GithubToday, PrimaryAndGitHub, IconOnly }`, `QList<MenuBarMode> offeredModes(bool providers, bool github)`, `MenuBarMode resolvedMode(MenuBarMode, const QList<MenuBarMode> &offered)`, `struct MenuBarProvider { QString id, displayName, symbol; qint64 staleAfterMs; std::optional<Snapshot<LimitsReport>> limits; }`, `struct MenuBarGitHub { int today, level, activeDays; bool stale; }`, `struct MenuBarLabel { QString text, title; std::optional<double> meter, companion; bool stale; QString badge; QString source; }`, `MenuBarLabel menuBarLabel(MenuBarMode, const QString &provider, const QList<MenuBarProvider> &, const std::optional<MenuBarGitHub> &, const QDateTime &now)`

- [ ] **Step 1: Write the failing tests**

`tests/core/tst_notificationplanner.cpp`:
```cpp
#include "core/notificationplanner.h"

#include <QTest>

using namespace contribusage;

namespace {

const QDateTime now(QDate(2026, 5, 9), QTime(12, 0), QTimeZone::UTC);
const QDateTime cycle = now.addSecs(3600);

LimitsReport report(double percent, const QString &provider = QStringLiteral("a"),
                    std::optional<QDateTime> resetsAt = cycle) {
    return {.provider = provider,
            .windows = {{.label = QStringLiteral("Current session"),
                         .kind = WindowKind::Session,
                         .usedPercent = percent,
                         .isBelowOne = false,
                         .resetsAt = resetsAt}},
            .billingNote = std::nullopt,
            .insights = std::nullopt,
            .rawOutput = std::nullopt};
}

QList<NotificationNote> notes(const LimitsReport &input, NotificationsSent &sent, const NotificationSettings &settings = {},
                              const QDateTime &at = now) {
    return planNotifications(input, QStringLiteral("Claude Code"), [](const QString &label) { return label; },
                             settings, sent, at);
}

QStringList titles(const LimitsReport &input, NotificationsSent &sent, const NotificationSettings &settings = {},
                   const QDateTime &at = now) {
    QStringList list;
    for (const NotificationNote &note : notes(input, sent, settings, at)) {
        list.append(note.title);
    }
    return list;
}

const NotificationSettings onReset{.thresholds = {80, 95}, .notifyOnReset = true};

} // namespace

// FR-13 to FR-15, US-3, ported from NotificationPlannerTests.swift.
class TestNotificationPlanner : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void eachThresholdFiresOncePerCycle() {
        NotificationsSent sent;
        QCOMPARE(titles(report(79), sent), QStringList());
        QCOMPARE(titles(report(80), sent), QStringList{QStringLiteral("Claude Code: Current session at 80 %")});
        QCOMPARE(titles(report(85), sent), QStringList());
        QCOMPARE(titles(report(96), sent), QStringList{QStringLiteral("Claude Code: Current session at 95 %")});
        QCOMPARE(titles(report(99), sent), QStringList());
        QCOMPARE(titles(report(99), sent, {.thresholds = {90, 95}, .notifyOnReset = false}), QStringList());
    }

    void aJumpOverTwoThresholdsSendsOnlyTheHighest() {
        NotificationsSent sent;
        QCOMPARE(titles(report(97), sent), QStringList{QStringLiteral("Claude Code: Current session at 95 %")});
        QCOMPARE(titles(report(99), sent), QStringList());
    }

    void aResetTimeMovingByUpToAnHourKeepsTheCycle() {
        NotificationsSent sent;
        const auto first = notes(report(85), sent, onReset);
        QCOMPARE(titles(report(85, QStringLiteral("a"), cycle.addSecs(-60)), sent, onReset), QStringList());
        QCOMPARE(titles(report(85, QStringLiteral("a"), cycle.addSecs(3600)), sent, onReset), QStringList());
        QCOMPARE(titles(report(85, QStringLiteral("a"), std::nullopt), sent, onReset), QStringList());
        const auto higher = notes(report(96, QStringLiteral("a"), cycle.addSecs(60)), sent, onReset);
        QCOMPARE(higher.size(), qsizetype(1));
        QCOMPARE(higher.at(0).title, QStringLiteral("Claude Code: Current session at 95 %"));
        QCOMPARE(higher.at(0).id, first.at(0).id);
        QCOMPARE(titles(report(10, QStringLiteral("a"), cycle.addSecs(3601)), sent, onReset),
                 QStringList{QStringLiteral("Claude Code: Current session has reset")});
    }

    void aCycleWithoutAResetTimeTakesTheFirstOneReported() {
        NotificationsSent sent;
        titles(report(85, QStringLiteral("a"), std::nullopt), sent);
        QCOMPARE(titles(report(85), sent), QStringList());
        QCOMPARE(titles(report(81, QStringLiteral("a"), cycle.addSecs(5 * 3600)), sent),
                 QStringList{QStringLiteral("Claude Code: Current session at 80 %")});
    }

    void aMissingResetTimeAfterTheCyclesResetEndsIt() {
        NotificationsSent sent;
        titles(report(85), sent, onReset);
        const QDateTime after = cycle.addSecs(60);
        QCOMPARE(titles(report(0.5, QStringLiteral("a"), std::nullopt), sent, onReset, after),
                 QStringList{QStringLiteral("Claude Code: Current session has reset")});
        QCOMPARE(titles(report(0.5, QStringLiteral("a"), std::nullopt), sent, onReset, after), QStringList());
        QCOMPARE(titles(report(81, QStringLiteral("a"), after.addSecs(5 * 3600)), sent, onReset, after),
                 QStringList{QStringLiteral("Claude Code: Current session at 80 %")});
    }

    void aResetReArmsAndNotifiesOnlyWhenAsked() {
        NotificationsSent sent;
        titles(report(90), sent);
        const QDateTime next = cycle.addSecs(5 * 3600);
        QCOMPARE(titles(report(10, QStringLiteral("a"), next), sent, {}, cycle), QStringList());
        QCOMPARE(titles(report(81, QStringLiteral("a"), next), sent, {}, cycle),
                 QStringList{QStringLiteral("Claude Code: Current session at 80 %")});
        NotificationsSent asked;
        titles(report(90), asked, onReset);
        QCOMPARE(titles(report(10, QStringLiteral("a"), next), asked, onReset, cycle),
                 QStringList{QStringLiteral("Claude Code: Current session has reset")});
        QCOMPARE(titles(report(10, QStringLiteral("a"), next), asked, onReset, cycle), QStringList());
    }

    void identicalLabelsFromTwoProvidersAreSeparate() {
        NotificationsSent sent;
        QCOMPARE(titles(report(85, QStringLiteral("a")), sent).size(), qsizetype(1));
        const auto other = notes(report(85, QStringLiteral("b")), sent);
        QCOMPARE(other.size(), qsizetype(1));
        QCOMPARE(other.at(0).provider, QStringLiteral("b"));
    }

    void thresholdsOutsideTheRangeAreIgnored() {
        NotificationsSent sent;
        const NotificationSettings settings{.thresholds = {10, 100, 60, 60, 70, 75, 90}, .notifyOnReset = false};
        QCOMPARE(titles(report(55), sent, settings), QStringList());
        QCOMPARE(titles(report(100), sent, settings), QStringList{QStringLiteral("Claude Code: Current session at 75 %")});
    }

    void oldKeysArePrunedAndResetWindowsWait() {
        NotificationsSent sent;
        titles(report(85, QStringLiteral("a"), std::nullopt), sent);
        QCOMPARE(titles(report(85, QStringLiteral("a"), std::nullopt), sent, {}, now.addSecs(8 * 86400)), QStringList());
        QCOMPARE(titles(report(85, QStringLiteral("a"), std::nullopt), sent, {}, now.addSecs(8 * 86400 + 1)).size(),
                 qsizetype(1));
        NotificationsSent passed;
        QCOMPARE(titles(report(85, QStringLiteral("a"), now), passed), QStringList());
    }

    void titlesUseTheWindowTitleAndKeysTheLabel() {
        NotificationsSent sent;
        const auto planned = planNotifications(
            report(80), QStringLiteral("Claude Code"),
            [](const QString &label) {
                return label == QStringLiteral("Current session") ? QStringLiteral("Aktuelle Sitzung") : label;
            },
            {}, sent, now);
        QCOMPARE(planned.at(0).title, QStringLiteral("Claude Code: Aktuelle Sitzung at 80 %"));
        QCOMPARE(sent.value(QStringLiteral("a")).firstKey(), QStringLiteral("Current session"));
    }
};

QTEST_GUILESS_MAIN(TestNotificationPlanner)
#include "tst_notificationplanner.moc"
```

`tests/core/tst_menubarlabel.cpp`:
```cpp
#include "core/menubarlabel.h"

#include <QLocale>
#include <QTest>

using namespace contribusage;

namespace {

const QDateTime now(QDate(2026, 5, 9), QTime(12, 0), QTimeZone::UTC);
const QString pad = QString(QChar(0x2007));

MenuBarProvider limits(const QString &id, std::initializer_list<std::pair<WindowKind, double>> windows,
                       const QString &symbol = QStringLiteral("sparkle"), qint64 ageMs = 0) {
    LimitsReport report{.provider = id, .windows = {}, .billingNote = {}, .insights = {}, .rawOutput = {}};
    for (const auto &[kind, percent] : windows) {
        report.windows.append({.label = QStringLiteral("w"),
                               .kind = kind,
                               .usedPercent = percent,
                               .isBelowOne = false,
                               .resetsAt = now.addSecs(3600)});
    }
    return {.id = id,
            .displayName = id,
            .symbol = symbol,
            .staleAfterMs = 1'800'000,
            .limits = Snapshot<LimitsReport>{.value = report, .fetchedAt = now.addMSecs(-ageMs), .origin = origins::poll()}};
}

MenuBarLabel label(MenuBarMode mode, const QList<MenuBarProvider> &providers, const QString &provider = {},
                   std::optional<MenuBarGitHub> github = std::nullopt) {
    return menuBarLabel(mode, provider.isEmpty() && !providers.isEmpty() ? providers.first().id : provider, providers,
                        github, now);
}

} // namespace

// FR-12, SPEC §11.1, ported from MenuBarLabelTests.swift.
class TestMenuBarLabel : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void initTestCase() { QLocale::setDefault(QLocale(QLocale::English, QLocale::UnitedStates)); }

    void primaryAndWeeklyShowTheMenuBarProvidersWindow() {
        const auto claude = limits(QStringLiteral("a"), {{WindowKind::Session, 23}, {WindowKind::Weekly, 51}, {WindowKind::Weekly, 70}});
        const MenuBarLabel primary = label(MenuBarMode::Primary, {claude});
        QCOMPARE(primary.text, QStringLiteral("23%"));
        QCOMPARE(primary.meter, std::optional(0.23));
        QCOMPARE(primary.companion, std::optional(0.51));
        QCOMPARE(primary.source, QStringLiteral("a"));
        const MenuBarLabel weekly = label(MenuBarMode::Weekly, {claude});
        QCOMPARE(weekly.text, QStringLiteral("51%"));
        QCOMPARE(weekly.companion, std::optional(0.23));
        const auto other = limits(QStringLiteral("b"), {{WindowKind::Session, 5}});
        QCOMPARE(label(MenuBarMode::Primary, {claude, other}, QStringLiteral("b")).text, QString(pad + QStringLiteral("5%")));
        QCOMPARE(label(MenuBarMode::Primary, {claude, other}, QStringLiteral("b")).companion, std::nullopt);
        QCOMPARE(label(MenuBarMode::Primary, {limits(QStringLiteral("a"), {{WindowKind::Session, 100}})}).text,
                 QStringLiteral("100%"));
    }

    // FR-11: a reset window fills nothing.
    void aResetWindowFillsNothing() {
        MenuBarProvider provider = limits(QStringLiteral("a"), {{WindowKind::Session, 80}});
        provider.limits->value.windows[0].resetsAt = now.addSecs(-1);
        QCOMPARE(label(MenuBarMode::Primary, {provider}).meter, std::nullopt);
    }

    void aMissingWindowFallsBackToTheHighestThenToUnknown() {
        const auto first = limits(QStringLiteral("a"), {{WindowKind::Weekly, 40}});
        const auto second = limits(QStringLiteral("b"), {{WindowKind::Session, 12}, {WindowKind::Weekly, 77}},
                                   QStringLiteral("terminal"));
        const MenuBarLabel highest = label(MenuBarMode::Primary, {first, second});
        QCOMPARE(highest.text, QStringLiteral("77%"));
        QCOMPARE(highest.title, QStringLiteral("b"));
        QCOMPARE(highest.badge, QStringLiteral("terminal"));
        QCOMPARE(highest.companion, std::optional(0.12));
        QCOMPARE(label(MenuBarMode::Highest, {first}).badge, QString());
        MenuBarProvider empty = limits(QStringLiteral("a"), {});
        empty.limits.reset();
        const MenuBarLabel unknown = label(MenuBarMode::Primary, {empty});
        QCOMPARE(unknown.text, QString(pad + pad + QStringLiteral("?")));
        QCOMPARE(unknown.meter, std::nullopt);
        QCOMPARE(label(MenuBarMode::Weekly, {limits(QStringLiteral("a"), {{WindowKind::Session, 5}})}).text,
                 QString(pad + QStringLiteral("5%")));
    }

    void staleValuesAreMarked() {
        const MenuBarLabel stale =
            label(MenuBarMode::Primary, {limits(QStringLiteral("a"), {{WindowKind::Session, 23}}, QStringLiteral("s"), 1'801'000)});
        QCOMPARE(stale.text, QStringLiteral("~23%"));
        QVERIFY(stale.stale);
        const MenuBarLabel github = label(MenuBarMode::GithubToday, {}, {}, MenuBarGitHub{.today = 7, .level = 2, .activeDays = 3, .stale = true});
        QCOMPARE(github.text, QStringLiteral("~7"));
        QVERIFY(github.stale);
    }

    void gitHubModesShowTodaysContributions() {
        const auto first = limits(QStringLiteral("a"), {{WindowKind::Session, 23}});
        const MenuBarGitHub github{.today = 7, .level = 2, .activeDays = 3, .stale = false};
        const MenuBarLabel today = label(MenuBarMode::GithubToday, {first}, {}, github);
        QCOMPARE(today.text, QStringLiteral("7"));
        QCOMPARE(today.title, QStringLiteral("GitHub"));
        QCOMPARE(today.meter, std::optional(0.5));
        QCOMPARE(today.source, QStringLiteral("github"));
        QCOMPARE(label(MenuBarMode::GithubToday, {first}).text, QStringLiteral("?"));
        const MenuBarLabel both = label(MenuBarMode::PrimaryAndGitHub, {first}, {}, github);
        QCOMPARE(both.text, QStringLiteral("23% · 7"));
        QCOMPARE(both.meter, std::optional(0.23));
    }

    void iconOnlyKeepsTheMeters() {
        const MenuBarLabel icon = label(MenuBarMode::IconOnly,
                                        {limits(QStringLiteral("a"), {{WindowKind::Session, 23}, {WindowKind::Weekly, 51}})});
        QCOMPARE(icon.text, QString());
        QCOMPARE(icon.meter, std::optional(0.23));
        QCOMPARE(label(MenuBarMode::IconOnly, {}).title, QStringLiteral("contribusage"));
    }

    void onlyModesOfSourcesThatAreOnAreOffered() {
        using M = MenuBarMode;
        QCOMPARE(offeredModes(true, true),
                 (QList<M>{M::Primary, M::Weekly, M::Highest, M::GithubToday, M::PrimaryAndGitHub, M::IconOnly}));
        QCOMPARE(offeredModes(true, false), (QList<M>{M::Primary, M::Weekly, M::Highest, M::IconOnly}));
        QCOMPARE(offeredModes(false, true), (QList<M>{M::GithubToday, M::IconOnly}));
        QCOMPARE(resolvedMode(M::PrimaryAndGitHub, offeredModes(true, false)), M::Primary);
        QCOMPARE(resolvedMode(M::Weekly, offeredModes(false, true)), M::GithubToday);
        QCOMPARE(resolvedMode(M::GithubToday, offeredModes(false, false)), M::IconOnly);
        QCOMPARE(resolvedMode(M::Weekly, offeredModes(true, false)), M::Weekly);
    }
};

QTEST_GUILESS_MAIN(TestMenuBarLabel)
#include "tst_menubarlabel.moc"
```

Append to `tests/CMakeLists.txt`:
```cmake

ecm_add_test(core/tst_notificationplanner.cpp
    TEST_NAME tst_notificationplanner
    LINK_LIBRARIES contribusage_core Qt6::Test)

ecm_add_test(core/tst_menubarlabel.cpp
    TEST_NAME tst_menubarlabel
    LINK_LIBRARIES contribusage_core Qt6::Test)
```

- [ ] **Step 2: Run them and confirm they fail**

Run: `cmake -B build -G Ninja && cmake --build build`
Expected: FAIL with `core/notificationplanner.h: No such file or directory`.

- [ ] **Step 3: Write the planner and the label**

`src/core/notificationplanner.h`:
```cpp
#pragma once

#include "core/reports.h"

#include <QDateTime>
#include <QList>
#include <QMap>
#include <QString>

#include <functional>
#include <optional>

namespace contribusage {

// FR-15: what was sent in one window's reset cycle; the highest threshold stands for every lower one.
struct NotificationCycle {
    std::optional<QDateTime> resetsAt;
    int threshold = 0;
    QDateTime sentAt; // FR-15 prunes the cycle 8 days later

    bool operator==(const NotificationCycle &) const = default;
};

// The cycles by provider and window label, as state.json will store them (L-5).
using NotificationsSent = QMap<QString, QMap<QString, NotificationCycle>>;

struct NotificationNote {
    QString id;       // one per window and cycle: provider|label|reset seconds or "-" (FR-52)
    QString provider; // the notification's group
    QString title;    // starts with the provider's display name

    bool operator==(const NotificationNote &) const = default;
};

// `notificationThresholds` and `notifyOnReset` (SPEC §10.7).
struct NotificationSettings {
    QList<int> thresholds{80, 95};
    bool notifyOnReset = false;
};

// FR-13 to FR-15, ADR-027, ADR-038: the notifications a fetched report brings. Updates `sent` and drops cycles sent
// more than 8 days ago. Pure: the applet delivers the notes (L-5).
QList<NotificationNote> planNotifications(const LimitsReport &report, const QString &displayName,
                                          const std::function<QString(const QString &)> &windowTitle,
                                          const NotificationSettings &settings, NotificationsSent &sent,
                                          const QDateTime &now);

} // namespace contribusage
```

`src/core/notificationplanner.cpp`:
```cpp
#include "core/notificationplanner.h"

#include <algorithm>
#include <cmath>

namespace contribusage {

namespace {

NotificationNote note(const QString &provider, const QString &label, const std::optional<QDateTime> &resetsAt,
                      const QString &title) {
    const QString reset = resetsAt ? QString::number(resetsAt->toSecsSinceEpoch()) : QStringLiteral("-");
    return {.id = QString(provider + u'|' + label + u'|' + reset), .provider = provider, .title = title};
}

} // namespace

QList<NotificationNote> planNotifications(const LimitsReport &report, const QString &displayName,
                                          const std::function<QString(const QString &)> &windowTitle,
                                          const NotificationSettings &settings, NotificationsSent &sent,
                                          const QDateTime &now) {
    // FR-13: up to 3 values between 50 and 99.
    QList<int> thresholds;
    for (const int value : settings.thresholds) {
        if (value >= 50 && value <= 99 && !thresholds.contains(value)) {
            thresholds.append(value);
        }
    }
    std::ranges::sort(thresholds);
    thresholds = thresholds.mid(0, 3);
    for (auto provider = sent.begin(); provider != sent.end();) {
        provider->removeIf([&now](const auto &entry) { return entry.value().sentAt.secsTo(now) > 8 * 86400; });
        provider = provider->isEmpty() ? sent.erase(provider) : std::next(provider);
    }
    QMap<QString, NotificationCycle> cycles = sent.value(report.provider);
    QList<NotificationNote> notes;
    for (const UsageWindow &window : report.windows) {
        if (window.resetsAt && *window.resetsAt <= now) {
            continue; // FR-11: unknown until the next refresh
        }
        const QString title = windowTitle(window.label);
        std::optional<NotificationCycle> cycle;
        if (cycles.contains(window.label)) {
            cycle = cycles.value(window.label);
        }
        // FR-15, ADR-038: only a move of more than 1 h ends the cycle; a window without a reset time ends it once the
        // cycle's own time has passed; a cycle without one is kept.
        if (cycle && cycle->resetsAt) {
            const QDateTime was = *cycle->resetsAt;
            const bool ended = window.resetsAt ? std::abs(was.secsTo(*window.resetsAt)) > 3600 : now > was;
            if (ended) {
                if (settings.notifyOnReset) {
                    notes.append(note(report.provider, window.label, was,
                                      QString(displayName + QStringLiteral(": ") + title + QStringLiteral(" has reset"))));
                }
                cycle.reset();
            }
        }
        // The cycle keeps its first known reset time, so its notes share one identifier (FR-52).
        const std::optional<QDateTime> resetsAt = cycle && cycle->resetsAt ? cycle->resetsAt : window.resetsAt;
        const int floor = cycle ? cycle->threshold : 0;
        std::optional<int> highest;
        for (const int threshold : std::as_const(thresholds)) {
            if (window.usedPercent >= threshold && threshold > floor) {
                highest = threshold;
            }
        }
        if (highest) {
            cycle = NotificationCycle{.resetsAt = resetsAt, .threshold = *highest, .sentAt = now};
            notes.append(note(report.provider, window.label, resetsAt,
                              QString(displayName + QStringLiteral(": ") + title + QStringLiteral(" at ") +
                                      QString::number(*highest) + QStringLiteral(" %"))));
        } else if (cycle) {
            cycle->resetsAt = resetsAt;
        }
        if (cycle) {
            cycles.insert(window.label, *cycle);
        } else {
            cycles.remove(window.label);
        }
    }
    if (cycles.isEmpty()) {
        sent.remove(report.provider);
    } else {
        sent.insert(report.provider, cycles);
    }
    return notes;
}

} // namespace contribusage
```

`src/core/menubarlabel.h`:
```cpp
#pragma once

#include "core/reports.h"
#include "core/snapshot.h"

#include <QDateTime>
#include <QList>
#include <QString>

#include <optional>

namespace contribusage {

// FR-12's display modes (saved as `menuBarMode` in L-5).
enum class MenuBarMode { Primary, Weekly, Highest, GithubToday, PrimaryAndGitHub, IconOnly };

// FR-12: the modes whose sources are on; `IconOnly` always is.
QList<MenuBarMode> offeredModes(bool providers, bool github);
// The mode the label shows: `mode` while offered, else the first offered one.
MenuBarMode resolvedMode(MenuBarMode mode, const QList<MenuBarMode> &offered);

// An enabled provider and its last limits.
struct MenuBarProvider {
    QString id;
    QString displayName;
    QString symbol;
    qint64 staleAfterMs = 0;
    std::optional<Snapshot<LimitsReport>> limits;
};

// GitHub's today: contributions, today's level (0 to 4), this week's days with contributions.
struct MenuBarGitHub {
    int today = 0;
    int level = 0;
    int activeDays = 0;
    bool stale = false;
};

// SPEC §11.1: what the panel label shows.
struct MenuBarLabel {
    QString text;  // "23%" padded with figure spaces to two digits, "~23%", "?", "7", or empty for IconOnly
    QString title; // the source's name; "contribusage" with every source off
    std::optional<double> meter;     // the shown window's fraction; nullopt without one or while it is reset (FR-11)
    std::optional<double> companion; // the provider's other window; GitHub: active days of 7
    bool stale = false;
    QString badge;  // the provider's symbol when Highest chose among several providers (SPEC §7.7)
    QString source; // a provider ID or "github"; empty with every source off
};

// `mode` is resolved; `providers` are the enabled ones in registry order; `provider` is the label's provider.
MenuBarLabel menuBarLabel(MenuBarMode mode, const QString &provider, const QList<MenuBarProvider> &providers,
                          const std::optional<MenuBarGitHub> &github, const QDateTime &now);

} // namespace contribusage
```

`src/core/menubarlabel.cpp`:
```cpp
#include "core/menubarlabel.h"

#include "core/display.h"

#include <QLocale>

#include <algorithm>

namespace contribusage {

namespace {

bool isReset(const UsageWindow &window, const QDateTime &now) {
    return window.resetsAt && *window.resetsAt <= now;
}

std::optional<double> fraction(const UsageWindow &window, const QDateTime &now) {
    if (isReset(window, now)) {
        return std::nullopt;
    }
    return std::clamp(window.usedPercent / 100.0, 0.0, 1.0);
}

// SPEC §11.1: figure spaces (as wide as a digit) in front up to two digits, so the label ends at its value.
QString padded(const QString &text) {
    const auto digits = std::ranges::count_if(text, [](QChar c) { return c.isDigit(); });
    return QString(QChar(0x2007)).repeated(std::max<qsizetype>(0, 2 - digits)) + text;
}

MenuBarLabel windowLabel(const UsageWindow &window, const MenuBarProvider &provider, const QString &badge,
                         const QDateTime &now) {
    const bool stale = provider.limits && provider.limits->fetchedAt.msecsTo(now) > provider.staleAfterMs;
    const WindowKind otherKind = window.kind == WindowKind::Weekly ? WindowKind::Session : WindowKind::Weekly;
    std::optional<double> companion;
    for (const UsageWindow &other : provider.limits->value.windows) {
        if (other.kind == otherKind && !(other == window)) {
            companion = fraction(other, now);
            break;
        }
    }
    return {.text = padded(QString((stale ? QStringLiteral("~") : QString()) +
                                   percentText(window.usedPercent, window.isBelowOne, window.resetsAt, now))),
            .title = provider.displayName,
            .meter = fraction(window, now),
            .companion = companion,
            .stale = stale,
            .badge = badge,
            .source = provider.id};
}

// The provider's first window of `kind`, else the highest window across providers, else "?" (FR-12, US-1).
MenuBarLabel limitsLabel(std::optional<WindowKind> kind, const QString &providerId,
                         const QList<MenuBarProvider> &providers, const QDateTime &now) {
    const MenuBarProvider *provider = nullptr;
    for (const MenuBarProvider &candidate : providers) {
        if (candidate.id == providerId) {
            provider = &candidate;
        }
    }
    if (kind && provider && provider->limits) {
        for (const UsageWindow &window : provider->limits->value.windows) {
            if (window.kind == *kind) {
                return windowLabel(window, *provider, {}, now);
            }
        }
    }
    const UsageWindow *highest = nullptr;
    const MenuBarProvider *owner = nullptr;
    for (const MenuBarProvider &candidate : providers) {
        if (!candidate.limits) {
            continue;
        }
        for (const UsageWindow &window : candidate.limits->value.windows) {
            if (!isReset(window, now) && (!highest || window.usedPercent > highest->usedPercent)) {
                highest = &window;
                owner = &candidate;
            }
        }
    }
    if (!highest) {
        const MenuBarProvider *shown = provider ? provider : (providers.isEmpty() ? nullptr : &providers.first());
        return {.text = padded(QStringLiteral("?")),
                .title = shown ? shown->displayName : QStringLiteral("contribusage"),
                .meter = std::nullopt,
                .companion = std::nullopt,
                .stale = false,
                .badge = {},
                .source = shown ? shown->id : QString()};
    }
    return windowLabel(*highest, *owner, providers.size() > 1 ? owner->symbol : QString(), now);
}

} // namespace

QList<MenuBarMode> offeredModes(bool providers, bool github) {
    QList<MenuBarMode> modes;
    for (const MenuBarMode mode : {MenuBarMode::Primary, MenuBarMode::Weekly, MenuBarMode::Highest,
                                   MenuBarMode::GithubToday, MenuBarMode::PrimaryAndGitHub, MenuBarMode::IconOnly}) {
        const bool usesProviders = mode == MenuBarMode::Primary || mode == MenuBarMode::Weekly ||
                                   mode == MenuBarMode::Highest || mode == MenuBarMode::PrimaryAndGitHub;
        const bool usesGitHub = mode == MenuBarMode::GithubToday || mode == MenuBarMode::PrimaryAndGitHub;
        if ((providers || !usesProviders) && (github || !usesGitHub)) {
            modes.append(mode);
        }
    }
    return modes;
}

MenuBarMode resolvedMode(MenuBarMode mode, const QList<MenuBarMode> &offered) {
    return offered.contains(mode) ? mode : offered.first();
}

MenuBarLabel menuBarLabel(MenuBarMode mode, const QString &provider, const QList<MenuBarProvider> &providers,
                          const std::optional<MenuBarGitHub> &github, const QDateTime &now) {
    const QString githubText =
        github ? QString((github->stale ? QStringLiteral("~") : QString()) + QLocale().toString(github->today))
               : QStringLiteral("?");
    switch (mode) {
    case MenuBarMode::Primary:
        return limitsLabel(WindowKind::Session, provider, providers, now);
    case MenuBarMode::Weekly:
        return limitsLabel(WindowKind::Weekly, provider, providers, now);
    case MenuBarMode::Highest:
        return limitsLabel(std::nullopt, provider, providers, now);
    case MenuBarMode::GithubToday:
        return {.text = githubText,
                .title = QStringLiteral("GitHub"),
                .meter = github ? std::optional(github->level / 4.0) : std::nullopt,
                .companion = github ? std::optional(github->activeDays / 7.0) : std::nullopt,
                .stale = github && github->stale,
                .badge = {},
                .source = QStringLiteral("github")};
    case MenuBarMode::PrimaryAndGitHub: {
        MenuBarLabel label = limitsLabel(WindowKind::Session, provider, providers, now);
        label.text += QStringLiteral(" · ") + githubText;
        return label;
    }
    case MenuBarMode::IconOnly: {
        if (providers.isEmpty()) {
            return {.text = {}, .title = QStringLiteral("contribusage"), .meter = {}, .companion = {}, .stale = false,
                    .badge = {}, .source = {}};
        }
        MenuBarLabel label = limitsLabel(WindowKind::Session, provider, providers, now);
        label.text.clear();
        return label;
    }
    }
    return {};
}

} // namespace contribusage
```

In `src/core/CMakeLists.txt`, add `menubarlabel.cpp menubarlabel.h notificationplanner.cpp notificationplanner.h` after `limitscache.h`.

- [ ] **Step 4: Run the tests and confirm they pass**

Run: `cmake --build build && ctest --test-dir build --output-on-failure -R "tst_notificationplanner|tst_menubarlabel"`
Expected: PASS.

- [ ] **Step 5: Format, full suite, commit**

Run: `clang-format -i src/core/notificationplanner.* src/core/menubarlabel.* tests/core/tst_notificationplanner.cpp tests/core/tst_menubarlabel.cpp && clang-format --dry-run --Werror $(git ls-files -co --exclude-standard '*.cpp' '*.h') && QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure`
Expected: no format output; all tests pass.

```bash
git add src/core tests
git commit -m "Core: notification planner and menu bar label

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 6: Refresh, popup trigger, conditions and label in the widget

**Files:**
- Modify: `src/claudecode/limitsview.cpp`, `tests/claudecode/tst_limitsview.cpp`, `src/applet/popupmodel.h`, `src/applet/popupmodel.cpp`, `src/applet/Footer.qml`, `src/applet/main.qml`, `tests/applet/tst_appletqml.cpp`

**Interfaces:**
- Consumes: Tasks 3 to 5.
- Produces: `PopupModel` gains `Q_INVOKABLE void refresh()` and `Q_INVOKABLE void popupOpened()`; the Footer's Refresh button is enabled and calls `PopupModel.refresh()`; `main.qml` calls `PopupModel.popupOpened()` when the popup opens; `labelView` goes through `menuBarLabel` (Primary mode, SPEC §11.1 padding and `~`).

- [ ] **Step 1: Write the failing tests**

In `tests/claudecode/tst_limitsview.cpp`, replace `labelShowsTheSessionOrUnknown` with:
```cpp
    // SPEC §11.1: the session's percent; "?" padded to two digits without data; "~" when stale.
    void labelShowsTheSessionOrUnknown() {
        const QVariantMap loaded = labelView(SourceState<LimitsReport>::loaded(snapshot(60)), now);
        QCOMPARE(loaded.value(QStringLiteral("text")).toString(), QStringLiteral("23%"));
        QCOMPARE(loaded.value(QStringLiteral("fraction")).toDouble(), 0.23);
        const QVariantMap unknown = labelView(SourceState<LimitsReport>::loading(std::nullopt), now);
        QCOMPARE(unknown.value(QStringLiteral("text")).toString(),
                 QString(QString(QChar(0x2007)).repeated(2) + QStringLiteral("?")));
        QCOMPARE(unknown.value(QStringLiteral("fraction")).toDouble(), 0.0);
        const QVariantMap stale = labelView(SourceState<LimitsReport>::loaded(snapshot(31 * 60)), now);
        QCOMPARE(stale.value(QStringLiteral("text")).toString(), QStringLiteral("~23%"));
    }
```
(`snapshot(seconds)` is the test file's helper that makes a snapshot fetched that many seconds before `now`.)

In `tests/applet/tst_appletqml.cpp`, add:
```cpp
    // SPEC §11.2: the footer's Refresh is live.
    void refreshButtonIsEnabled() {
        QQmlEngine engine;
        QQmlComponent component(&engine,
                                QUrl(QStringLiteral("qrc:/qt/qml/plasma/applet/org/contribusage/widget/Footer.qml")));
        std::unique_ptr<QObject> footer(component.create());
        QVERIFY2(footer, qPrintable(component.errorString()));
        auto *refresh = footer->findChild<QObject *>(QStringLiteral("refreshButton"));
        QVERIFY(refresh);
        QVERIFY(refresh->property("enabled").toBool());
    }
```

- [ ] **Step 2: Run them and confirm they fail**

Run: `cmake --build build && QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure -R "tst_limitsview|tst_appletqml"`
Expected: FAIL in `labelShowsTheSessionOrUnknown` (`"?"` against the padded text) and in `refreshButtonIsEnabled` (`refresh` is null).

- [ ] **Step 3: Wire it**

`src/claudecode/limitsview.cpp` — add `#include "core/menubarlabel.h"` and replace `labelView` with:
```cpp
QVariantMap labelView(const SourceState<LimitsReport> &state, const QDateTime &now) {
    const MenuBarProvider provider{.id = providerId(),
                                   .displayName = QStringLiteral("Claude Code"),
                                   .symbol = QStringLiteral("draw-star"),
                                   .staleAfterMs = LimitsJob::defaultPolicy().staleAfterMs,
                                   .limits = state.snapshot()};
    const MenuBarLabel label = menuBarLabel(MenuBarMode::Primary, provider.id, {provider}, std::nullopt, now);
    const double meter = label.meter.value_or(0.0);
    return {{QStringLiteral("text"), label.text},
            {QStringLiteral("fraction"), meter},
            {QStringLiteral("level"), meterLevelName(meterLevel(meter * 100))},
            {QStringLiteral("stale"), label.stale}};
}
```
and add `#include "claudecode/limitsjob.h"` (for the policy) and, if `providerId()` is not visible in this file, `#include "claudecode/claudecodelimits.h"`.

`src/applet/Footer.qml` — on the Refresh button: add `objectName: "refreshButton"`, remove the comment `// Wired to the refresh coordinator in L-5.` and `enabled: false`, and add `onClicked: PopupModel.refresh()`.

`src/applet/main.qml` — inside `PlasmoidItem`, add:
```qml
    // SPEC §12: the popup opening is a trigger for sources whose data is old (ADR-019).
    onExpandedChanged: if (expanded) PopupModel.popupOpened()
```

`src/applet/popupmodel.h`:
- forward-declare `class ScheduleConditionsSource; class SystemConditions;` in namespace `contribusage`
- add `Q_INVOKABLE void refresh();` and `Q_INVOKABLE void popupOpened();`
- add the members, before `m_runner`:
```cpp
    std::unique_ptr<contribusage::ScheduleConditionsSource> m_conditions;
    std::unique_ptr<contribusage::SystemConditions> m_system;
```

`src/applet/popupmodel.cpp`:
- add the includes `core/sourceschedule.h` and `platform/systemconditions.h`
- at the start of the live branch of the constructor (before `m_runner` is made), add:
```cpp
    m_conditions = std::make_unique<ScheduleConditionsSource>();
    m_system = std::make_unique<SystemConditions>(*m_conditions);
```
- construct the limits job as `std::make_unique<claudecode::LimitsJob>(*m_limits, AppPaths::live(), claudecode::LimitsJob::defaultPolicy(), m_conditions.get())` and the GitHub job as `std::make_unique<github::GitHubJob>(*m_githubAccount, AppPaths::live(), github::GitHubJob::defaultPolicy(), m_conditions.get())`
- add:
```cpp
void PopupModel::refresh() {
    if (m_fake) {
        return;
    }
    m_job->refresh();
    m_githubJob->refresh();
    m_activityJob->rescan();
}

void PopupModel::popupOpened() {
    if (m_fake) {
        return;
    }
    m_job->popupOpened();
    m_githubJob->popupOpened();
    m_activityJob->rescan(); // SPEC §12: transcripts on popup open
}
```
- in the destructor, after the jobs are reset, add `m_system.reset();` and `m_conditions.reset();` (the jobs' schedules hold a QPointer to the conditions, so the order is safe either way).

- [ ] **Step 4: Run every test and look at it**

Run: `cmake --build build && QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure`
Expected: all tests pass.

Restart the viewer and take a screenshot after 15 s; then click Refresh in the viewer (`ydotool` is not assumed: check by watching `~/.local/share/contribusage/state.json`'s `limits` `fetchedAt` change after a manual refresh is impossible without input, so instead confirm in the screenshot that the Refresh icon is no longer greyed out). Also check the panel label with `plasmoidviewer -c org.kde.panel -a org.contribusage.widget -f horizontal -l topedge -s 500x220`.
Expected: Refresh enabled; label "NN%" as before; no QML warnings.

- [ ] **Step 5: Format and commit**

```bash
clang-format -i src/applet/*.cpp src/applet/*.h src/claudecode/limitsview.cpp tests/claudecode/tst_limitsview.cpp tests/applet/tst_appletqml.cpp
git add src tests
git commit -m "Applet: Refresh button, popup trigger, suspend and network conditions, label per SPEC §11.1

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 7: Contract and vault

**Files:**
- Create: `llm-wiki/decisions/0045-one-schedule-per-source.md`
- Modify: `SPEC.md` (§17.8 L-1c and L-5 lines), `llm-wiki/modules/core.md`, `llm-wiki/modules/claude-code.md`, `llm-wiki/modules/github.md`, `llm-wiki/modules/applet.md`, `llm-wiki/index.md`, `llm-wiki/log.md`

- [ ] **Step 1: Contract**

In `SPEC.md` §17.8:
- replace the `L-1c` line with:
```markdown
- [x] **L-1c** The scheduling decision (§12 rule 7) and a schedule per polled source (ADR-045) with backoff, manual floor, popup and reset-time triggers, the 6 h unsupported-plan re-check, suspend and network conditions from logind and QNetworkInformation; the notification planner (FR-13 to FR-15) and the menu bar label (FR-12, §11.1) in the core; the Refresh button. Accept: `tst_schedule`, `tst_sourceschedule`, `tst_systemconditions`, `tst_notificationplanner` and `tst_menubarlabel` pass; the job tests run on their schedules.
```
- replace the `L-5` line with:
```markdown
- [ ] **L-5** Settings pages and notifications: notification delivery through KNotification with the planner's cycles persisted in `state.json`, the popup layout (`PopoverLayout`, FR-44 to FR-47), the label mode and style settings (FR-12, FR-51), intervals and thresholds; Low Power Mode from power-profiles-daemon.
```

- [ ] **Step 2: ADR-045**

`llm-wiki/decisions/0045-one-schedule-per-source.md` (today's date for `YYYY-MM-DD`):
```markdown
---
type: decision
status: accepted
updated: YYYY-MM-DD
aliases: [ADR-045]
tracks: [src/core/sourceschedule.cpp, src/core/schedule.cpp]
tags: [scheduling, architecture]
---
# ADR-045: One schedule object per polled source

- **Decided:** YYYY-MM-DD · **Spec:** SPEC §12, ADR-018, ADR-022 · **Supersedes:** —

## Context
The macOS app had one `RefreshCoordinator` actor that polled every source, consumed pushes and planned notifications (ADR-018). In the Plasma widget each source already is a job object with its own cache and state, the process runner already serialises child processes (NFR-18), and Qt timers are cheap.

## Options
| Option | For | Against |
|---|---|---|
| One coordinator object | Mirrors the Swift design | Re-creates per-source state inside a second object; more code between the jobs and their timer |
| A `SourceSchedule` per job, fed by shared conditions | Each job owns its whole life cycle; the pure `nextRun` stays the single decision | Cross-source ordering relies on the process runner's queue |

## Decision
Every polled job owns a `SourceSchedule` that arms one timer from `nextRun` (SPEC §12 rule 7) and emits `due()`. The machine's conditions live in one `ScheduleConditionsSource` that the platform's `SystemConditions` feeds from logind and QNetworkInformation. Rule 5's registry order is the process runner's FIFO.

## Consequences
- Notification planning hooks into the limits job when delivery arrives (L-5).
- Low Power Mode stays off until power-profiles-daemon is read (L-5).
```

- [ ] **Step 3: Vault**

- `llm-wiki/modules/core.md`: `updated:` today; extend the C++ note with `schedule.h` (rule 7), `sourceschedule.h` (ADR-045), `notificationplanner.h` (FR-13 to FR-15, delivery in L-5) and `menubarlabel.h` (FR-12, §11.1).
- `llm-wiki/modules/claude-code.md` and `llm-wiki/modules/github.md`: `updated:` today; say the jobs run on a `SourceSchedule` with their SPEC §12 policy (`defaultPolicy()`), `refresh()` is the manual refresh and `popupOpened()` the popup trigger.
- `llm-wiki/modules/applet.md`: `updated:` today; add to `## Contract`: `PopupModel.refresh()` (Footer's Refresh) and `popupOpened()` (from `main.qml`), and that the schedules share `ScheduleConditionsSource` fed by `SystemConditions`.
- `llm-wiki/index.md`: under Decisions, after ADR-044, add `- [[decisions/0045-one-schedule-per-source]]: ADR-045, one schedule object per polled source instead of one coordinator`.
- `llm-wiki/log.md`, append:
```markdown

## [YYYY-MM-DD] feat | Schedule, refresh and planning (L-1c)
- `src/core/`: SPEC §12's `nextRun`, a `SourceSchedule` per polled source (ADR-045), the notification planner and the menu bar label.
- The limits and GitHub jobs follow their §12 policies with backoff, triggers and the manual floor; suspend and network come from logind and QNetworkInformation; the Refresh button works.
```

- [ ] **Step 4: Run every check and commit**

Run: `llm-wiki/tools/wiki.sh lint && cmake --build build && QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure && clang-format --dry-run --Werror $(git ls-files '*.cpp' '*.h') && echo all-ok`
Expected: `wiki lint: clean`, all tests pass, then `all-ok`.

```bash
git add SPEC.md llm-wiki
git commit -m "Spec: L-1c done; ADR-045 one schedule per source

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```
