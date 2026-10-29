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
