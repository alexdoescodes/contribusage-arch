#include "core/limitscache.h"
#include "github/githubjob.h"
#include "support/fakehttptransport.h"
#include "support/fakesecretstore.h"

#include <QFile>
#include <QTemporaryDir>
#include <QTest>

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
        GitHubJob job(fixture.account, fixture.paths(), 60'000);
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
        GitHubJob job(fixture.account, fixture.paths(), 60'000);
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
        GitHubJob job(fixture.account, fixture.paths(), 60'000);
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
        GitHubJob job(fixture.account, fixture.paths(), 60'000);
        job.start();
        QCOMPARE(job.state().phase, Phase::Loading);
        QVERIFY(job.state().snapshot() == cached);
    }

    void failureKeepsThePreviousSnapshot() {
        Fixture fixture;
        fixture.secrets.values.insert(QStringLiteral("token"), QStringLiteral("ghp_x"));
        fixture.transport.respond(200, ::fixture("calendar"));
        GitHubJob job(fixture.account, fixture.paths(), 60'000);
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
        GitHubJob job(fixture.account, fixture.paths(), 60'000);
        job.start();
        QTRY_COMPARE(job.state().phase, Phase::Failed);
        QCOMPARE(job.state().error->kind, SourceError::Kind::Unauthorized);
        const qsizetype sent = fixture.transport.requests.size();
        job.refresh();
        QTest::qWait(50);
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
        GitHubJob job(fixture.account, fixture.paths(), 100);
        job.start();
        QTRY_COMPARE(job.state().phase, Phase::Failed);
        QCOMPARE(job.state().error->kind, SourceError::Kind::RateLimited);
        const qsizetype sent = fixture.transport.requests.size();
        QTest::qWait(400); // several intervals pass, the reset does not
        QCOMPARE(fixture.transport.requests.size(), sent);
    }
};

QTEST_GUILESS_MAIN(TestGitHubJob)
#include "tst_githubjob.moc"
