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
        .insights =
            Insights{.note = QStringLiteral("Approximate."),
                     .periods = {InsightsPeriod{
                         .label = QStringLiteral("Last 24h"),
                         .summary = QStringLiteral("206 requests · 5 sessions"),
                         .shares = {Share{.label = QStringLiteral("At >150k context"), .percent = 62},
                                    Share{.label = QStringLiteral("unreadable line"), .percent = std::nullopt}},
                         .rankings = {Ranking{.title = QStringLiteral("Skills"),
                                              .items = {Share{.label = QStringLiteral("/skill-a"), .percent = 35}}}}}}},
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
            .days = {ActivityDay{
                .day = QDate(2026, 10, 4),
                .requests = 668,
                .sessions = 8,
                .tokens =
                    TokenCounts{.input = 12'000, .output = 812'000, .cacheWrite = 300'000, .cacheRead = 4'200'000},
                .byModel = {{QStringLiteral("claude-opus-5-5"),
                             TokenCounts{.input = 12'000, .output = 0, .cacheWrite = 0, .cacheRead = 0}}}}},
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

    // A field of the wrong type rejects the whole report, so a foreign history.json is never loaded as zeros.
    void activityReportRejectsWronglyTypedFields_data() {
        QTest::addColumn<QString>("field");
        QTest::addColumn<QJsonValue>("value");
        QTest::newRow("requests as text") << QStringLiteral("requests") << QJsonValue(QStringLiteral("5"));
        QTest::newRow("sessions missing") << QStringLiteral("sessions") << QJsonValue(QJsonValue::Undefined);
        QTest::newRow("tokens field as text") << QStringLiteral("tokens")
                                              << QJsonValue(QJsonObject{{QStringLiteral("input"), QStringLiteral("x")},
                                                                        {QStringLiteral("output"), 0},
                                                                        {QStringLiteral("cacheWrite"), 0},
                                                                        {QStringLiteral("cacheRead"), 0}});
        QTest::newRow("byModel not an object") << QStringLiteral("byModel") << QJsonValue(7);
    }
    void activityReportRejectsWronglyTypedFields() {
        QFETCH(QString, field);
        QFETCH(QJsonValue, value);
        QJsonObject json = toJson(ActivityReport{
            .provider = QStringLiteral("claude-code"),
            .days = {ActivityDay{.day = QDate(2026, 10, 4), .requests = 1, .sessions = 1, .tokens = {}, .byModel = {}}},
            .skippedLines = 0});
        QJsonArray days = json.value(QStringLiteral("days")).toArray();
        QJsonObject day = days.at(0).toObject();
        if (value.isUndefined()) {
            day.remove(field);
        } else {
            day.insert(field, value);
        }
        days.replace(0, day);
        json.insert(QStringLiteral("days"), days);
        QVERIFY(!activityReportFromJson(json));
    }

    void activityReportRejectsTextSkippedLines() {
        QJsonObject json =
            toJson(ActivityReport{.provider = QStringLiteral("claude-code"), .days = {}, .skippedLines = 0});
        json.insert(QStringLiteral("skippedLines"), QStringLiteral("3"));
        QVERIFY(!activityReportFromJson(json));
    }

    void limitsReportRejectsATextBelowOneFlag() {
        QJsonObject json = toJson(sampleLimits());
        QJsonArray windows = json.value(QStringLiteral("windows")).toArray();
        QJsonObject first = windows.at(0).toObject();
        first.insert(QStringLiteral("isBelowOne"), QStringLiteral("yes"));
        windows.replace(0, first);
        json.insert(QStringLiteral("windows"), windows);
        QVERIFY(!limitsReportFromJson(json));
    }

    // FR-38: insights have at least one period, each with a summary.
    void limitsReportRejectsInsightsWithoutPeriodsOrSummary() {
        QJsonObject json = toJson(sampleLimits());
        QJsonObject insights = json.value(QStringLiteral("insights")).toObject();
        insights.insert(QStringLiteral("periods"), QJsonArray());
        json.insert(QStringLiteral("insights"), insights);
        QVERIFY(!limitsReportFromJson(json));

        json = toJson(sampleLimits());
        insights = json.value(QStringLiteral("insights")).toObject();
        QJsonArray periods = insights.value(QStringLiteral("periods")).toArray();
        QJsonObject period = periods.at(0).toObject();
        period.remove(QStringLiteral("summary"));
        periods.replace(0, period);
        insights.insert(QStringLiteral("periods"), periods);
        json.insert(QStringLiteral("insights"), insights);
        QVERIFY(!limitsReportFromJson(json));
    }
};

QTEST_GUILESS_MAIN(TestReports)
#include "tst_reports.moc"
