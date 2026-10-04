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
