#include "claudecode/limitsjob.h"
#include "core/limitscache.h"
#include "support/fakeprocessrunner.h"

#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

using namespace contribusage;
using namespace contribusage::claudecode;

namespace {

const QString shell = QStringLiteral("/usr/bin/zsh");
const QString claude = QStringLiteral("/home/octocat/.local/bin/claude");

QString fixture(const char *name) {
    QFile file(QStringLiteral(CONTRIBUSAGE_FIXTURES "/usage/") + QString::fromLatin1(name) + QStringLiteral(".txt"));
    return file.open(QIODevice::ReadOnly) ? QString::fromUtf8(file.readAll()) : QString();
}

struct Fixture {
    QTemporaryDir temporary;
    FakeProcessRunner runner;
    ClaudeLocator locator{runner, QStringLiteral("/home/octocat"), shell};
    ClaudeCodeLimits limits{runner, locator, AppPaths{.root = temporary.path()}};

    Fixture() {
        runner.answer(shell, {}, FakeProcessRunner::exited(0, claude + QStringLiteral("\n/usr/bin\n")));
        runner.answer(claude, QStringLiteral("--version"), FakeProcessRunner::exited(0, QStringLiteral("2.1.289\n")));
    }
    AppPaths paths() const { return {.root = temporary.path()}; }
    qsizetype probes() const {
        qsizetype count = 0;
        for (const auto &request : runner.requests) {
            count += request.arguments.value(0) == u"-p";
        }
        return count;
    }
};

using Phase = SourceState<LimitsReport>::Phase;

} // namespace

// The probe on an interval, the cache (FR-10) and SPEC §11.3's states.
class TestLimitsJob : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void loadsAndCaches() {
        Fixture fixture;
        fixture.runner.answer(claude, QStringLiteral("-p"),
                              FakeProcessRunner::exited(0, ::fixture("subscription-basic")));
        LimitsJob job(fixture.limits, fixture.paths(), 60'000);
        QSignalSpy changed(&job, &LimitsJob::stateChanged);
        job.start();
        QCOMPARE(job.state().phase, Phase::Loading);
        QTRY_COMPARE(job.state().phase, Phase::Loaded);
        QCOMPARE(job.state().snapshot()->value.windows.size(), 2);
        QVERIFY(limitscache::load(fixture.paths(), QStringLiteral("claude-code")));
        QVERIFY(changed.count() >= 2);
    }

    void startsWithTheCachedSnapshot() {
        Fixture fixture;
        const Snapshot<LimitsReport> cached{.value = LimitsReport{.provider = QStringLiteral("claude-code"),
                                                                  .windows = {},
                                                                  .billingNote = {},
                                                                  .insights = {},
                                                                  .rawOutput = {}},
                                            .fetchedAt = QDateTime::currentDateTimeUtc().addSecs(-3600),
                                            .origin = origins::poll()};
        QVERIFY(limitscache::save(fixture.paths(), cached));
        LimitsJob job(fixture.limits, fixture.paths(), 60'000);
        job.start();
        QCOMPARE(job.state().phase, Phase::Loading);
        QVERIFY(job.state().snapshot() == cached);
    }

    // A failed probe keeps the previous snapshot to show dimmed.
    void failureKeepsThePreviousSnapshot() {
        Fixture fixture;
        fixture.runner.answer(claude, QStringLiteral("-p"),
                              FakeProcessRunner::exited(0, ::fixture("subscription-basic")));
        LimitsJob job(fixture.limits, fixture.paths(), 60'000);
        job.start();
        QTRY_COMPARE(job.state().phase, Phase::Loaded);
        fixture.runner.rules.last().outcome = std::unexpected(SourceError{.kind = SourceError::Kind::TimedOut});
        job.refresh();
        QTRY_COMPARE(job.state().phase, Phase::Failed);
        QCOMPARE(job.state().error->kind, SourceError::Kind::TimedOut);
        QVERIFY(job.state().snapshot());
    }

    void notFoundAndUnsupportedPlanAreNotConfigured() {
        Fixture missing;
        missing.runner.rules.clear();
        LimitsJob missingJob(missing.limits, missing.paths(), 60'000);
        missingJob.start();
        QTRY_COMPARE(missingJob.state().phase, Phase::NotConfigured);
        QCOMPARE(missingJob.state().reason, NotConfiguredReason::ToolNotInstalled);

        Fixture apiKey;
        apiKey.runner.answer(claude, QStringLiteral("-p"), FakeProcessRunner::exited(0, ::fixture("not-subscription")));
        LimitsJob apiKeyJob(apiKey.limits, apiKey.paths(), 60'000);
        apiKeyJob.start();
        QTRY_COMPARE(apiKeyJob.state().phase, Phase::NotConfigured);
        QCOMPARE(apiKeyJob.state().reason, NotConfiguredReason::UnsupportedPlan);
        QCOMPARE(apiKeyJob.state().note, QStringLiteral("Total cost:            $0.0000"));
    }

    void probesAgainOnTheInterval() {
        Fixture fixture;
        fixture.runner.answer(claude, QStringLiteral("-p"),
                              FakeProcessRunner::exited(0, ::fixture("subscription-basic")));
        LimitsJob job(fixture.limits, fixture.paths(), 100);
        job.start();
        QTRY_VERIFY(fixture.probes() >= 3);
    }

    void aDeletedJobGetsNoLateResult() {
        Fixture fixture;
        fixture.runner.answer(claude, QStringLiteral("-p"),
                              FakeProcessRunner::exited(0, ::fixture("subscription-basic")));
        auto *job = new LimitsJob(fixture.limits, fixture.paths(), 60'000);
        job->start();
        delete job;
        QTest::qWait(100); // the fake's answers arrive now and must not touch the deleted job
        QVERIFY(!limitscache::load(fixture.paths(), QStringLiteral("claude-code")));
    }
};

QTEST_GUILESS_MAIN(TestLimitsJob)
#include "tst_limitsjob.moc"
