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
