#include "core/limitscache.h"
#include "github/githubjob.h"
#include "github/githubview.h"
#include "support/fakehttptransport.h"
#include "support/fakesecretstore.h"

#include <QFile>
#include <QTemporaryDir>
#include <QTest>

#include "core/sourceschedule.h"

using namespace contribusage;
using namespace contribusage::github;

namespace {

QByteArray fixture(const char *name) {
    QFile file(QStringLiteral(CONTRIBUSAGE_FIXTURES "/github/") + QString::fromLatin1(name) + QStringLiteral(".json"));
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
}

struct Fixture {
    QTemporaryDir temporary;
    FakeHttpTransport transport;
    FakeSecretStore secrets;
    GitHubClient client{transport, QStringLiteral("1.0")};
    GitHubAccount account{secrets, client};

    AppPaths paths() const { return {.root = temporary.path()}; }
};

using Phase = SourceState<GitHubReport>::Phase;

} // namespace

// SPEC §12 (30 min), §11.3, ADR-020, ADR-022, FR-10 for GitHub.
class TestGitHubJob : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void withoutATokenItIsNotConfigured() {
        Fixture fixture;
        GitHubJob job(fixture.account, fixture.paths(), SchedulePolicy::every(60'000));
        job.start();
        QTRY_COMPARE(job.state().phase, Phase::NotConfigured);
        QCOMPARE(job.state().reason, NotConfiguredReason::GithubTokenMissing);
        QVERIFY(fixture.transport.requests.isEmpty());
    }

    void loadsAndCachesBesideTheLimits() {
        Fixture fixture;
        fixture.secrets.values.insert(QStringLiteral("token"), QStringLiteral("ghp_x"));
        fixture.transport.respond(200, ::fixture("calendar"));
        QVERIFY(limitscache::save(fixture.paths(), {.value = LimitsReport{.provider = QStringLiteral("claude-code"),
                                                                          .windows = {},
                                                                          .billingNote = {},
                                                                          .insights = {},
                                                                          .rawOutput = {}},
                                                    .fetchedAt = QDateTime::currentDateTimeUtc(),
                                                    .origin = origins::poll()}));
        GitHubJob job(fixture.account, fixture.paths(), SchedulePolicy::every(60'000));
        job.start();
        QTRY_COMPARE(job.state().phase, Phase::Loaded);
        QCOMPARE(job.state().snapshot()->value.calendar.login, QStringLiteral("octocat"));
        QVERIFY(githubcache::load(fixture.paths()));
        QVERIFY(limitscache::load(fixture.paths(), QStringLiteral("claude-code"))); // the limits stay
    }

    // Review Focus 4: the token is never written to the cache.
    void cacheHoldsNoToken() {
        Fixture fixture;
        fixture.secrets.values.insert(QStringLiteral("token"), QStringLiteral("ghp_secret_value"));
        fixture.transport.respond(200, ::fixture("calendar"));
        GitHubJob job(fixture.account, fixture.paths(), SchedulePolicy::every(60'000));
        job.start();
        QTRY_COMPARE(job.state().phase, Phase::Loaded);
        QFile state(fixture.temporary.path() + QStringLiteral("/state.json"));
        QVERIFY(state.open(QIODevice::ReadOnly));
        QVERIFY(!state.readAll().contains("ghp_secret_value"));
    }

    void startsWithTheCachedSnapshot() {
        Fixture fixture;
        const Snapshot<GitHubReport> cached{
            .value =
                GitHubReport{.calendar = {.login = QStringLiteral("octocat"), .weeks = {}, .totalContributions = 1},
                             .stats = {}},
            .fetchedAt = QDateTime::currentDateTimeUtc().addSecs(-3600),
            .origin = origins::github()};
        QVERIFY(githubcache::save(fixture.paths(), cached));
        GitHubJob job(fixture.account, fixture.paths(), SchedulePolicy::every(60'000));
        job.start();
        QCOMPARE(job.state().phase, Phase::Loading);
        QVERIFY(job.state().snapshot() == cached);
    }

    void failureKeepsThePreviousSnapshot() {
        Fixture fixture;
        fixture.secrets.values.insert(QStringLiteral("token"), QStringLiteral("ghp_x"));
        fixture.transport.respond(200, ::fixture("calendar"));
        GitHubJob job(fixture.account, fixture.paths(), SchedulePolicy::every(60'000));
        job.start();
        QTRY_COMPARE(job.state().phase, Phase::Loaded);
        fixture.transport.failure = SourceError{.kind = SourceError::Kind::Offline};
        job.refresh();
        QTRY_COMPARE(job.state().phase, Phase::Failed);
        QCOMPARE(job.state().error->kind, SourceError::Kind::Offline);
        QVERIFY(job.state().snapshot());
    }

    // ADR-022: a 401 stops the polling until the token changes.
    void unauthorizedPausesUntilTheTokenChanges() {
        Fixture fixture;
        fixture.secrets.values.insert(QStringLiteral("token"), QStringLiteral("ghp_old"));
        fixture.transport.respond(401, "{}");
        GitHubJob job(fixture.account, fixture.paths(), SchedulePolicy::every(100));
        job.start();
        QTRY_COMPARE(job.state().phase, Phase::Failed);
        QCOMPARE(job.state().error->kind, SourceError::Kind::Unauthorized);
        const qsizetype sent = fixture.transport.requests.size();
        QTest::qWait(400); // several intervals pass
        QCOMPARE(fixture.transport.requests.size(), sent);
        fixture.transport.respond(200, ::fixture("calendar"));
        job.tokenChanged();
        QTRY_COMPARE(job.state().phase, Phase::Loaded);
    }

    // ADR-020: no request before the reset time.
    void rateLimitWaitsForTheReset() {
        Fixture fixture;
        fixture.secrets.values.insert(QStringLiteral("token"), QStringLiteral("ghp_x"));
        const qint64 reset = QDateTime::currentSecsSinceEpoch() + 3600;
        fixture.transport.respond(403, "{}",
                                  {{"x-ratelimit-remaining", "0"}, {"x-ratelimit-reset", QByteArray::number(reset)}});
        GitHubJob job(fixture.account, fixture.paths(), SchedulePolicy::every(100));
        job.start();
        QTRY_COMPARE(job.state().phase, Phase::Failed);
        QCOMPARE(job.state().error->kind, SourceError::Kind::RateLimited);
        const qsizetype sent = fixture.transport.requests.size();
        QTest::qWait(400); // several intervals pass, the reset does not
        QCOMPARE(fixture.transport.requests.size(), sent);
    }

    // ADR-022: a fetch still running when the token changes finishes into nothing; its 401 does not pause the new
    // token.
    void staleFetchAfterTokenChangeIsDropped() {
        Fixture fixture;
        fixture.secrets.values.insert(QStringLiteral("token"), QStringLiteral("ghp_old"));
        fixture.transport.respond(401, "{}");
        GitHubJob job(fixture.account, fixture.paths(), SchedulePolicy::every(60'000));
        job.start();
        QTRY_COMPARE(fixture.transport.requests.size(), qsizetype(1));
        fixture.transport.respond(200, ::fixture("calendar"));
        job.tokenChanged();
        QTRY_COMPARE(job.state().phase, Phase::Loaded);
        QTest::qWait(50);
        QCOMPARE(job.state().phase, Phase::Loaded);
    }

    // ADR-022: a token change drops the old account from the view and from state.json.
    void tokenChangeDropsTheCachedAccount() {
        Fixture fixture;
        fixture.secrets.values.insert(QStringLiteral("token"), QStringLiteral("ghp_x"));
        fixture.transport.respond(200, ::fixture("calendar"));
        GitHubJob job(fixture.account, fixture.paths(), SchedulePolicy::every(60'000));
        job.start();
        QTRY_COMPARE(job.state().phase, Phase::Loaded);
        fixture.secrets.values.clear();
        job.tokenChanged();
        QVERIFY(!job.state().snapshot());
        QVERIFY(!githubcache::load(fixture.paths()));
        QTRY_COMPARE(job.state().phase, Phase::NotConfigured);
    }

    // A disconnect the wallet refuses is shown, and the account stays.
    void disconnectFailureIsShown() {
        Fixture fixture;
        fixture.secrets.values.insert(QStringLiteral("token"), QStringLiteral("ghp_x"));
        fixture.transport.respond(200, ::fixture("calendar"));
        GitHubJob job(fixture.account, fixture.paths(), SchedulePolicy::every(60'000));
        job.start();
        QTRY_COMPARE(job.state().phase, Phase::Loaded);
        fixture.secrets.failure =
            SourceError{.kind = SourceError::Kind::Io, .text = QStringLiteral("KWallet is not available")};
        job.disconnectAccount();
        QTRY_COMPARE(job.state().phase, Phase::Failed);
        QCOMPARE(job.state().error->kind, SourceError::Kind::Io);
        QVERIFY(job.state().snapshot());
        QVERIFY(fixture.secrets.values.contains(QStringLiteral("token")));
    }

    // A denied wallet or a missing token is not asked again on every interval; a token change asks again.
    void walletFailureAndMissingTokenPausePolling_data() {
        QTest::addColumn<bool>("denied");
        QTest::newRow("wallet denied") << true;
        QTest::newRow("no token") << false;
    }
    void walletFailureAndMissingTokenPausePolling() {
        QFETCH(bool, denied);
        Fixture fixture;
        if (denied) {
            fixture.secrets.failure = SourceError{.kind = SourceError::Kind::Io};
        }
        GitHubJob job(fixture.account, fixture.paths(), SchedulePolicy::every(100));
        job.start();
        QTRY_COMPARE(fixture.secrets.reads, 1);
        QTest::qWait(400); // several intervals
        QCOMPARE(fixture.secrets.reads, 1);
        job.tokenChanged();
        QTRY_COMPARE(fixture.secrets.reads, 2);
    }

    // ADR-022: a manual refresh runs while paused; ADR-020: never before the rate limit's reset.
    void manualRefreshRunsWhilePausedButNotBeforeTheReset() {
        Fixture fixture;
        fixture.secrets.values.insert(QStringLiteral("token"), QStringLiteral("ghp_x"));
        fixture.transport.respond(401, "{}");
        GitHubJob job(fixture.account, fixture.paths(), SchedulePolicy::every(60'000));
        job.start();
        QTRY_COMPARE(job.state().phase, Phase::Failed);
        fixture.transport.respond(200, ::fixture("calendar"));
        job.refresh();
        QTRY_COMPARE(job.state().phase, Phase::Loaded);
        const qint64 reset = QDateTime::currentSecsSinceEpoch() + 3600;
        fixture.transport.respond(403, "{}",
                                  {{"x-ratelimit-remaining", "0"}, {"x-ratelimit-reset", QByteArray::number(reset)}});
        job.refresh();
        QTRY_COMPARE(job.state().phase, Phase::Failed);
        const qsizetype sent = fixture.transport.requests.size();
        job.refresh();
        QTest::qWait(100);
        QCOMPARE(fixture.transport.requests.size(), sent);
    }

    // Review Focus 3, FR-43: off sends nothing, hides the block and keeps the token.
    void disabledGitHubSendsNothing() {
        Fixture fixture;
        fixture.secrets.values.insert(QStringLiteral("token"), QStringLiteral("ghp_x"));
        fixture.transport.respond(200, ::fixture("calendar"));
        GitHubJob job(fixture.account, fixture.paths(), SchedulePolicy::every(100));
        job.setEnabled(false);
        job.start();
        QTest::qWait(300);
        QVERIFY(fixture.transport.requests.isEmpty());
        QCOMPARE(job.state().phase, Phase::NotConfigured);
        QCOMPARE(job.state().reason, NotConfiguredReason::ProviderDisabled);
        QVERIFY(githubView(job.state(), QDateTime::currentDateTime()).isEmpty());
        QVERIFY(fixture.secrets.values.contains(QStringLiteral("token")));
        job.setEnabled(true);
        QTRY_COMPARE(job.state().phase, Phase::Loaded);
    }
};

QTEST_GUILESS_MAIN(TestGitHubJob)
#include "tst_githubjob.moc"
