#include "core/heatmap.h"

#include <QTest>
#include <QVariantMap>

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

    // FR-48: every layer's stripe and text per cell, in layer order; a day without text reads "no data".
    void heatmapViewStacksLayers() {
        const QDate today(2026, 10, 5);
        const HeatmapLayer claude{.name = QStringLiteral("Claude Code"),
                                  .hue = QStringLiteral("claude"),
                                  .levels = {{today, 4}},
                                  .texts = {{today, QStringLiteral("1.2M tokens")}}};
        const HeatmapLayer github{.name = QStringLiteral("GitHub"),
                                  .hue = QStringLiteral("github"),
                                  .levels = {{today, 2}, {today.addDays(-1), 0}},
                                  .texts = {{today, QStringLiteral("3 contributions")},
                                            {today.addDays(-1), QStringLiteral("0 contributions")}}};
        const QVariantMap view = heatmapView({claude, github}, today);
        QCOMPARE(view.value(QStringLiteral("layers")).toList().size(), qsizetype(2));
        QCOMPARE(view.value(QStringLiteral("columns")).toInt(), 26);
        const QVariantList cells = view.value(QStringLiteral("cells")).toList();
        const QVariantMap last = cells.last().toMap();
        QCOMPARE(last.value(QStringLiteral("line")).toString(),
                 QStringLiteral("2026-10-05 · Claude Code: 1.2M tokens · GitHub: 3 contributions"));
        const QVariantList stripes = last.value(QStringLiteral("stripes")).toList();
        QCOMPARE(stripes.size(), qsizetype(2));
        QCOMPARE(stripes.at(0).toMap().value(QStringLiteral("hue")).toString(), QStringLiteral("claude"));
        QCOMPARE(stripes.at(1).toMap().value(QStringLiteral("level")).toInt(), 2);
        const QVariantMap yesterday = cells.at(cells.size() - 2).toMap();
        QCOMPARE(yesterday.value(QStringLiteral("line")).toString(),
                 QStringLiteral("2026-10-04 · Claude Code: no data · GitHub: 0 contributions"));
        QVERIFY(yesterday.value(QStringLiteral("stripes")).toList().isEmpty());
        QVERIFY(heatmapView({}, today).isEmpty());
    }
};

QTEST_GUILESS_MAIN(TestHeatmap)
#include "tst_heatmap.moc"
