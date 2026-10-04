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
        row("recent past stays", "resets Sep 27 at 4am (Europe/Berlin)", "2026-09-27T20:00:00Z",
            "2026-09-27T02:00:00Z");
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
        QCOMPARE(report.billingNote, std::optional(QStringLiteral(
                                         "You are currently using your subscription to power your Claude Code usage")));
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
        QVERIFY(period.shares ==
                (QList<Share>{{.label = QStringLiteral("Mostly one project"), .percent = std::nullopt}}));
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
