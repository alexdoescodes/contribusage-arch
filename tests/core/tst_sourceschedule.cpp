#include "core/sourceschedule.h"

#include <QElapsedTimer>
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

    // Review I-1: an interval override (the 6 h unsupported-plan re-check) still lets a manual refresh through.
    void anIntervalOverrideKeepsManualRefresh() {
        SourceSchedule schedule(policy(60'000, 60'000, 60'000, 0));
        QSignalSpy due(&schedule, &SourceSchedule::due);
        schedule.start();
        QTRY_COMPARE(due.size(), 1);
        schedule.finished(true);
        schedule.setIntervalOverride(6 * 3600'000);
        schedule.requestManual();
        QTRY_COMPARE(due.size(), 2);
    }

    // Review I-2: back online, a source that needs the network runs within its floor, not after its interval.
    void reconnectingRunsWithinTheFloor() {
        ScheduleConditionsSource conditions;
        SourceSchedule schedule(policy(60'000, 60'000, 60'000, 0, true), &conditions);
        QSignalSpy due(&schedule, &SourceSchedule::due);
        schedule.start();
        QTRY_COMPARE(due.size(), 1);
        schedule.finished(true);
        conditions.setConditions({.online = false, .lowPower = false, .asleep = false, .lastWake = std::nullopt});
        conditions.setConditions({});
        QTRY_COMPARE(due.size(), 2);
    }

    // Review I-2: a failure for lack of network does not grow the backoff.
    void aFailureWithoutBackoffKeepsTheInterval() {
        SourceSchedule schedule(policy(200, 200, 2'000, 0));
        QSignalSpy due(&schedule, &SourceSchedule::due);
        schedule.start();
        QTRY_COMPARE(due.size(), 1);
        schedule.finished(false, false);
        QElapsedTimer timer;
        timer.start();
        QTRY_COMPARE(due.size(), 2);
        QVERIFY(timer.elapsed() < 350);
    }

    // Review I-3: a run that never reports back ends at the deadline; its late answer is ignored.
    void aRunThatNeverAnswersEndsAtTheDeadline() {
        SourceSchedule schedule(SchedulePolicy::every(50));
        schedule.setRunDeadline(400);
        QSignalSpy due(&schedule, &SourceSchedule::due);
        schedule.start();
        QTRY_COMPARE(due.size(), 1);
        const quint64 first = schedule.currentRun();
        QTRY_COMPARE(due.size(), 2);          // deadline, then the interval
        schedule.finished(true, true, first); // late: ignored, the second run goes on
        QTest::qWait(120);
        QCOMPARE(due.size(), 2);
    }
};

QTEST_GUILESS_MAIN(TestSourceSchedule)
#include "tst_sourceschedule.moc"
