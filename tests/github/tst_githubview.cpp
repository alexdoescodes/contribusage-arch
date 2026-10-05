#include "core/heatmap.h"
#include "github/githubview.h"

#include <QTest>
#include <QTimeZone>

using namespace contribusage;
using namespace contribusage::github;

namespace {

const QDateTime now(QDate(2026, 10, 5), QTime(10, 0), QTimeZone::UTC);

Snapshot<GitHubReport> snapshot(qint64 ageSeconds) {
    QList<ContributionDay> week;
    for (int offset = 6; offset >= 0; --offset) {
        week.append({.date = now.date().addDays(-offset), .count = offset == 0 ? 4 : 1, .level = offset == 0 ? 3 : 1});
    }
    return {.value =
                GitHubReport{
                    .calendar = {.login = QStringLiteral("octocat"), .weeks = {week}, .totalContributions = 1917},
                    .stats =
                        {.today = 4, .thisWeek = 6, .currentStreak = 7, .streakNeedsToday = false, .longestStreak = 7}},
            .fetchedAt = now.addSecs(-ageSeconds),
            .origin = origins::github()};
}

} // namespace

class TestGitHubView : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void initTestCase() { QLocale::setDefault(QLocale(QLocale::English, QLocale::UnitedStates)); }

    void loadedShowsLoginAndStats() {
        const QVariantMap view = githubView(SourceState<GitHubReport>::loaded(snapshot(120)), now);
        QCOMPARE(view.value(QStringLiteral("login")).toString(), QStringLiteral("@octocat"));
        QCOMPARE(view.value(QStringLiteral("statsText")).toString(),
                 QStringLiteral("Today 4 · Streak 7 days · Year 1,917"));
        QCOMPARE(view.value(QStringLiteral("updatedText")).toString(), QStringLiteral("updated 2 min ago"));
        QVERIFY(view.value(QStringLiteral("connected")).toBool());
        QCOMPARE(view.value(QStringLiteral("statusText")).toString(), QString());
    }

    void notConfiguredAsksForAToken() {
        const QVariantMap view =
            githubView(SourceState<GitHubReport>::notConfigured(NotConfiguredReason::GithubTokenMissing), now);
        QVERIFY(!view.value(QStringLiteral("connected")).toBool());
        QCOMPARE(view.value(QStringLiteral("login")).toString(), QString());
    }

    void unauthorizedNamesTheToken() {
        const QVariantMap view =
            githubView(SourceState<GitHubReport>::failed({.kind = SourceError::Kind::Unauthorized}, snapshot(60)), now);
        QCOMPARE(view.value(QStringLiteral("statusText")).toString(),
                 QStringLiteral("GitHub token is invalid or expired"));
        QVERIFY(view.value(QStringLiteral("stale")).toBool());
    }

    // A rejected token brings the token form back (ADR-043); a working account does not show it.
    void needsTokenWhenMissingOrRejected() {
        using State = SourceState<GitHubReport>;
        QVERIFY(githubView(State::notConfigured(NotConfiguredReason::GithubTokenMissing), now)
                    .value(QStringLiteral("needsToken"))
                    .toBool());
        QVERIFY(githubView(State::failed({.kind = SourceError::Kind::Unauthorized}, snapshot(60)), now)
                    .value(QStringLiteral("needsToken"))
                    .toBool());
        QVERIFY(!githubView(State::failed({.kind = SourceError::Kind::Offline}, snapshot(60)), now)
                     .value(QStringLiteral("needsToken"))
                     .toBool());
        QVERIFY(!githubView(State::loaded(snapshot(60)), now).value(QStringLiteral("needsToken")).toBool());
    }

    // FR-20: GitHub's levels as the heatmap's green layer over the shown range.
    void heatmapLayer() {
        const QVariantMap heatmap = githubHeatmap(SourceState<GitHubReport>::loaded(snapshot(60)), now.date());
        const QVariantList cells = heatmap.value(QStringLiteral("cells")).toList();
        QCOMPARE(cells.size(), heatmapDays(now.date()).size());
        const QVariantMap today = cells.last().toMap();
        QCOMPARE(today.value(QStringLiteral("line")).toString(),
                 QStringLiteral("2026-10-05 · GitHub: 4 contributions"));
        const QVariantList stripes = today.value(QStringLiteral("stripes")).toList();
        QCOMPARE(stripes.size(), 1);
        QCOMPARE(stripes.first().toMap().value(QStringLiteral("level")).toInt(), 3);
        QCOMPARE(cells.first().toMap().value(QStringLiteral("stripes")).toList().size(), 0); // before the calendar
        QCOMPARE(heatmap.value(QStringLiteral("layers")).toList().size(), 1);
        QVERIFY(githubHeatmap(SourceState<GitHubReport>::loading(std::nullopt), now.date()).isEmpty());
    }

    void loadingIsASkeletonAndFailuresOfferRetry() {
        const QVariantMap loading = githubView(SourceState<GitHubReport>::loading(std::nullopt), now);
        QVERIFY(loading.value(QStringLiteral("skeleton")).toBool());
        QCOMPARE(loading.value(QStringLiteral("statusText")).toString(), QString());
        const QVariantMap offline =
            githubView(SourceState<GitHubReport>::failed({.kind = SourceError::Kind::Offline}, snapshot(60)), now);
        QCOMPARE(offline.value(QStringLiteral("action")).toMap().value(QStringLiteral("kind")).toString(),
                 QStringLiteral("retry"));
        const QVariantMap rejected =
            githubView(SourceState<GitHubReport>::failed({.kind = SourceError::Kind::Unauthorized}, snapshot(60)), now);
        QVERIFY(!rejected.contains(QStringLiteral("action"))); // the token form is the action (ADR-043)
    }

    // SPEC §11.3: a stale layer is dimmed like its section.
    void staleLayerIsDimmed() {
        QVERIFY(!githubLayer(SourceState<GitHubReport>::loaded(snapshot(60)), now)->dimmed);
        QVERIFY(githubLayer(SourceState<GitHubReport>::loaded(snapshot(3 * 3600)), now)->dimmed);
        QVERIFY(githubLayer(SourceState<GitHubReport>::failed({.kind = SourceError::Kind::Offline}, snapshot(60)), now)
                    ->dimmed);
    }
};

QTEST_GUILESS_MAIN(TestGitHubView)
#include "tst_githubview.moc"
