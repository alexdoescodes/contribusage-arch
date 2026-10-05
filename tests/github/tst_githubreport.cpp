#include "github/githubreport.h"

#include <QTest>

using namespace contribusage::github;

namespace {

// Consecutive days from `start`, one per count.
QList<ContributionDay> days(QDate start, std::initializer_list<int> counts) {
    QList<ContributionDay> list;
    for (const int count : counts) {
        list.append({.date = start, .count = count, .level = 0});
        start = start.addDays(1);
    }
    return list;
}

const QDate wednesday(2026, 9, 30);

} // namespace

// SPEC §8.4.4, FR-19, ported from ContributionStatsTests.swift.
class TestGitHubReport : public QObject {
    Q_OBJECT

private Q_SLOTS:
    // 09-14 … 09-16 a three-day run; 09-27 … 09-30 (today) the longest at four; 10-01 is after today and counts
    // nowhere. Monday first: 16 + 32 + 64; Sunday first adds 8.
    void statsFollowSpec_data() {
        QTest::addColumn<int>("firstDay");
        QTest::addColumn<int>("thisWeek");
        QTest::newRow("Monday first") << int(Qt::Monday) << 112;
        QTest::newRow("Sunday first") << int(Qt::Sunday) << 120;
    }
    void statsFollowSpec() {
        QFETCH(int, firstDay);
        QFETCH(int, thisWeek);
        const auto list = days(QDate(2026, 9, 14), {1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 2, 0, 0, 8, 16, 32, 64, 128});
        const ContributionStats stats = contributionStats(list, wednesday, Qt::DayOfWeek(firstDay));
        QVERIFY(
            stats ==
            (ContributionStats{
                .today = 64, .thisWeek = thisWeek, .currentStreak = 4, .streakNeedsToday = false, .longestStreak = 4}));
    }

    // Today 0 so far, or not in the calendar yet: the streak runs to yesterday and asks for today.
    void streakEndingYesterdayNeedsToday_data() {
        QTest::addColumn<QList<int>>("counts");
        QTest::newRow("today zero") << QList<int>{3, 5, 0};
        QTest::newRow("today missing") << QList<int>{3, 5};
    }
    void streakEndingYesterdayNeedsToday() {
        QFETCH(QList<int>, counts);
        QList<ContributionDay> list;
        QDate date(2026, 9, 28);
        for (const int count : counts) {
            list.append({.date = date, .count = count, .level = 0});
            date = date.addDays(1);
        }
        const ContributionStats stats = contributionStats(list, wednesday, Qt::Monday);
        QCOMPARE(stats.today, 0);
        QCOMPARE(stats.currentStreak, 2);
        QVERIFY(stats.streakNeedsToday);
    }

    void noStreakNeedsNothing() {
        const ContributionStats stats = contributionStats(days(QDate(2026, 9, 28), {3, 0, 0}), wednesday, Qt::Monday);
        QCOMPARE(stats.currentStreak, 0);
        QVERIFY(!stats.streakNeedsToday);
        QCOMPARE(stats.longestStreak, 1);
    }

    // The caller's today decides which entry is today (R-4 picks the zone).
    void todayDecidesTheEntry() {
        const auto list = days(QDate(2026, 9, 30), {1, 2});
        QCOMPARE(contributionStats(list, QDate(2026, 9, 30), Qt::Monday).today, 1);
        QCOMPARE(contributionStats(list, QDate(2026, 10, 1), Qt::Monday).today, 2);
    }

    void reportRoundTrips() {
        const GitHubReport report{
            .calendar = {.login = QStringLiteral("octocat"),
                         .weeks = {days(QDate(2026, 9, 27), {1, 0}), days(QDate(2026, 10, 4), {5})},
                         .totalContributions = 6},
            .stats = {.today = 5, .thisWeek = 5, .currentStreak = 1, .streakNeedsToday = false, .longestStreak = 1}};
        const auto decoded = gitHubReportFromJson(toJson(report));
        QVERIFY(decoded);
        QVERIFY(*decoded == report);
        QCOMPARE(report.calendar.days().size(), 3);
    }

    void reportRejectsAMissingLogin() {
        QJsonObject json = toJson(GitHubReport{});
        QJsonObject calendar = json.value(QStringLiteral("calendar")).toObject();
        calendar.remove(QStringLiteral("login"));
        json.insert(QStringLiteral("calendar"), calendar);
        QVERIFY(!gitHubReportFromJson(json));
    }
};

QTEST_GUILESS_MAIN(TestGitHubReport)
#include "tst_githubreport.moc"
