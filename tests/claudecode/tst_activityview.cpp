#include "claudecode/activityview.h"
#include "core/display.h"
#include "github/githubview.h"

#include <QLocale>
#include <QTest>

using namespace contribusage;
using namespace contribusage::claudecode;

namespace {

const QDate today(2026, 10, 5);

ActivityDay day(QDate date, qint64 input, qint64 requests = 1, qint64 sessions = 1) {
    return {.day = date,
            .requests = requests,
            .sessions = sessions,
            .tokens = {.input = input, .output = 2, .cacheWrite = 3, .cacheRead = 4},
            .byModel = {}};
}

SourceState<ActivityReport> loaded(QList<ActivityDay> days) {
    return SourceState<ActivityReport>::loaded(
        {.value = {.provider = QStringLiteral("claude-code"), .days = std::move(days), .skippedLines = 0},
         .fetchedAt = QDateTime::currentDateTimeUtc(),
         .origin = origins::activity()});
}

} // namespace

// US-5, SPEC §11.2 activity block, FR-48 heatmap layer.
class TestActivityView : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void initTestCase() { QLocale::setDefault(QLocale(QLocale::English, QLocale::UnitedStates)); }

    void todayAndCategories() {
        const QVariantMap view =
            activityView(loaded({day(today.addDays(-1), 100), day(today, 1'000'000, 668, 8)}), today);
        QCOMPARE(view.value(QStringLiteral("todayText")).toString(),
                 QString(QStringLiteral("Today: 668 requests · 8 sessions · ") + compactCount(1'000'009)));
        QCOMPARE(view.value(QStringLiteral("categoriesText")).toString(),
                 QString(QStringLiteral("Input ") + compactCount(1'000'000) + QStringLiteral(" · Output ") +
                         compactCount(2) + QStringLiteral(" · Cache read ") + compactCount(4) +
                         QStringLiteral(" · Cache write ") + compactCount(3)));
    }

    void weekBarsEndWithToday() {
        const QVariantMap view = activityView(loaded({day(today.addDays(-6), 491), day(today, 991)}), today);
        const QVariantList week = view.value(QStringLiteral("week")).toList();
        QCOMPARE(week.size(), qsizetype(7));
        QCOMPARE(week.at(0).toMap().value(QStringLiteral("fraction")).toDouble(), 0.5);
        QCOMPARE(week.at(3).toMap().value(QStringLiteral("fraction")).toDouble(), 0.0);
        QCOMPARE(week.at(6).toMap().value(QStringLiteral("fraction")).toDouble(), 1.0);
        QVERIFY(week.at(6).toMap().value(QStringLiteral("today")).toBool());
        QVERIFY(!week.at(5).toMap().value(QStringLiteral("today")).toBool());
    }

    void noActivityTodayReadsZero() {
        const QVariantMap view = activityView(loaded({day(today.addDays(-2), 5)}), today);
        QCOMPARE(view.value(QStringLiteral("todayText")).toString(),
                 QString(QStringLiteral("Today: 0 requests · 0 sessions · ") + compactCount(0)));
    }

    // SPEC §11.3: no transcripts is a message, the first scan a skeleton; the heatmap layer waits for data.
    void statesWithoutASnapshot() {
        const QVariantMap none =
            activityView(SourceState<ActivityReport>::notConfigured(NotConfiguredReason::NoLocalData), today);
        QCOMPARE(none.value(QStringLiteral("statusText")).toString(),
                 QStringLiteral("No Claude Code sessions found on this machine"));
        QVERIFY(activityView(SourceState<ActivityReport>::loading(std::nullopt), today)
                    .value(QStringLiteral("skeleton"))
                    .toBool());
        QVERIFY(!activityLayer(SourceState<ActivityReport>::loading(std::nullopt), today));
    }

    // FR-48: quartile levels over the shown range, the busiest day at 4; texts in tokens.
    void layerUsesQuartilesAndTokenTexts() {
        const auto layer = activityLayer(
            loaded({day(today.addDays(-400), 9'999'999), day(today.addDays(-1), 91), day(today, 991)}), today);
        QVERIFY(layer);
        QCOMPARE(layer->hue, QStringLiteral("claude"));
        QCOMPARE(layer->name, QStringLiteral("Claude Code"));
        QCOMPARE(layer->levels.value(today), 4);
        QVERIFY(layer->levels.value(today.addDays(-1)) < 4);
        QVERIFY(!layer->levels.contains(today.addDays(-400)));
        QCOMPARE(layer->texts.value(today), QString(compactCount(1000) + QStringLiteral(" tokens")));
    }

    // The widget's heatmap: Claude Code first, then GitHub, in one block.
    void layersCombineWithGitHub() {
        using GitHubState = SourceState<github::GitHubReport>;
        const github::GitHubReport report{.calendar = {.login = QStringLiteral("octocat"),
                                                       .weeks = {{{.date = today, .count = 3, .level = 2}}},
                                                       .totalContributions = 3},
                                          .stats = {}};
        const auto githubState = GitHubState::loaded(
            {.value = report, .fetchedAt = QDateTime::currentDateTimeUtc(), .origin = origins::github()});
        QList<HeatmapLayer> layers;
        for (const auto &layer : {activityLayer(loaded({day(today, 991)}), today),
                                  github::githubLayer(githubState, QDateTime::currentDateTimeUtc())}) {
            if (layer) {
                layers.append(*layer);
            }
        }
        const QVariantMap view = heatmapView(layers, today);
        QCOMPARE(view.value(QStringLiteral("cells")).toList().last().toMap().value(QStringLiteral("line")).toString(),
                 QString(QStringLiteral("2026-10-05 · Claude Code: ") + compactCount(1000) +
                         QStringLiteral(" tokens · GitHub: 3 contributions")));
    }

    // Review I-2, SPEC §11.3: a failed scan shows its error and Retry, with or without earlier data.
    void failuresShowAnErrorAndRetry() {
        const SourceError error{.kind = SourceError::Kind::Io, .text = QStringLiteral("unreadable")};
        const QVariantMap first = activityView(SourceState<ActivityReport>::failed(error, std::nullopt), today);
        QVERIFY(!first.value(QStringLiteral("skeleton")).toBool());
        QVERIFY(!first.value(QStringLiteral("statusText")).toString().isEmpty());
        QCOMPARE(first.value(QStringLiteral("action")).toMap().value(QStringLiteral("kind")).toString(),
                 QStringLiteral("retry"));
        const auto previous = loaded({day(today, 5)}).snapshot();
        const QVariantMap later = activityView(SourceState<ActivityReport>::failed(error, previous), today);
        QVERIFY(later.value(QStringLiteral("stale")).toBool());
        QVERIFY(!later.value(QStringLiteral("statusText")).toString().isEmpty());
        QVERIFY(!later.value(QStringLiteral("todayText")).toString().isEmpty());
    }
};

QTEST_GUILESS_MAIN(TestActivityView)
#include "tst_activityview.moc"
