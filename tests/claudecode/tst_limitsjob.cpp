#include "claudecode/limitsjob.h"
#include "core/limitscache.h"
#include "support/fakeprocessrunner.h"

#include <QFile>
#include <QLocale>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

#include "core/notificationstore.h"
#include "support/fakenotificationsink.h"

#include "core/sourceschedule.h"

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

// A session window at 85 % that resets two days from now, in Claude Code 2.1.289's comma format.
QString highUsage() {
    const QDateTime reset = QDateTime::currentDateTimeUtc().addDays(2);
    return QStringLiteral("You are currently using your subscription to power your Claude Code usage\n\n"
                          "Current session: 85% used · resets ") +
           QLocale::c().toString(reset, QStringLiteral("MMM d, h:mmap")) + QStringLiteral(" (UTC)\n");
}

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
        LimitsJob job(fixture.limits, fixture.paths(), SchedulePolicy::every(60'000));
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
        LimitsJob job(fixture.limits, fixture.paths(), SchedulePolicy::every(60'000));
        job.start();
        QCOMPARE(job.state().phase, Phase::Loading);
        QVERIFY(job.state().snapshot() == cached);
    }

    // A failed probe keeps the previous snapshot to show dimmed.
    void failureKeepsThePreviousSnapshot() {
        Fixture fixture;
        fixture.runner.answer(claude, QStringLiteral("-p"),
                              FakeProcessRunner::exited(0, ::fixture("subscription-basic")));
        LimitsJob job(fixture.limits, fixture.paths(), SchedulePolicy::every(60'000));
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
        LimitsJob missingJob(missing.limits, missing.paths(), SchedulePolicy::every(60'000));
        missingJob.start();
        QTRY_COMPARE(missingJob.state().phase, Phase::NotConfigured);
        QCOMPARE(missingJob.state().reason, NotConfiguredReason::ToolNotInstalled);

        Fixture apiKey;
        apiKey.runner.answer(claude, QStringLiteral("-p"), FakeProcessRunner::exited(0, ::fixture("not-subscription")));
        LimitsJob apiKeyJob(apiKey.limits, apiKey.paths(), SchedulePolicy::every(60'000));
        apiKeyJob.start();
        QTRY_COMPARE(apiKeyJob.state().phase, Phase::NotConfigured);
        QCOMPARE(apiKeyJob.state().reason, NotConfiguredReason::UnsupportedPlan);
        QCOMPARE(apiKeyJob.state().note, QStringLiteral("Total cost:            $0.0000"));
    }

    void probesAgainOnTheInterval() {
        Fixture fixture;
        fixture.runner.answer(claude, QStringLiteral("-p"),
                              FakeProcessRunner::exited(0, ::fixture("subscription-basic")));
        LimitsJob job(fixture.limits, fixture.paths(), SchedulePolicy::every(100));
        job.start();
        QTRY_VERIFY(fixture.probes() >= 3);
    }

    void aDeletedJobGetsNoLateResult() {
        Fixture fixture;
        fixture.runner.answer(claude, QStringLiteral("-p"),
                              FakeProcessRunner::exited(0, ::fixture("subscription-basic")));
        auto *job = new LimitsJob(fixture.limits, fixture.paths(), SchedulePolicy::every(60'000));
        job->start();
        delete job;
        QTest::qWait(100); // the fake's answers arrive now and must not touch the deleted job
        QVERIFY(!limitscache::load(fixture.paths(), QStringLiteral("claude-code")));
    }

    // Review Focus 1: pressing Refresh again within the floor runs once.
    void manualRefreshRunsOnceWithinTheFloor() {
        Fixture fixture;
        fixture.runner.answer(claude, QStringLiteral("-p"),
                              FakeProcessRunner::exited(0, ::fixture("subscription-basic")));
        SchedulePolicy policy = SchedulePolicy::every(60'000);
        policy.manualFloorMs = 60'000;
        LimitsJob job(fixture.limits, fixture.paths(), policy);
        job.start();
        QTRY_COMPARE(job.state().phase, Phase::Loaded);
        const qsizetype probes = fixture.probes();
        job.refresh();
        job.refresh();
        QTest::qWait(100);
        QCOMPARE(fixture.probes(), probes);
    }

    // Review Focus 4: a window's reset time is a trigger.
    void aResetTimeTriggersARun() {
        Fixture fixture;
        fixture.runner.answer(claude, QStringLiteral("-p"),
                              FakeProcessRunner::exited(0, ::fixture("subscription-basic")));
        SchedulePolicy policy = SchedulePolicy::every(3'600'000);
        policy.minimumMs = 0;
        LimitsJob job(fixture.limits, fixture.paths(), policy);
        job.start();
        QTRY_COMPARE(job.state().phase, Phase::Loaded);
        const qsizetype probes = fixture.probes();
        job.addResetTrigger(QDateTime::currentDateTimeUtc().addMSecs(200));
        QTRY_VERIFY(fixture.probes() > probes);
    }

    void policiesFollowSpec() {
        const SchedulePolicy limits = LimitsJob::defaultPolicy();
        QCOMPARE(limits.defaultMs, qint64(5 * 60'000));
        QCOMPARE(limits.minimumMs, qint64(60'000));
        QCOMPARE(limits.maximumMs, qint64(60 * 60'000));
        QCOMPARE(limits.manualFloorMs, qint64(30'000));
        QVERIFY(limits.needsNetwork);
    }

    // Review I-1: on an unsupported plan the 6 h re-check does not block a manual refresh.
    void unsupportedPlanStillRefreshesManually() {
        Fixture fixture;
        fixture.runner.answer(claude, QStringLiteral("-p"),
                              FakeProcessRunner::exited(0, ::fixture("not-subscription")));
        LimitsJob job(fixture.limits, fixture.paths(), SchedulePolicy::every(60'000));
        job.start();
        QTRY_COMPARE(job.state().phase, Phase::NotConfigured);
        const qsizetype probes = fixture.probes();
        job.refresh();
        QTRY_VERIFY(fixture.probes() > probes);
    }

    // FR-13: crossing a threshold notifies once; Review Focus 4: what the sink does is not the planner's business.
    void notesGoToTheSink() {
        Fixture fixture;
        fixture.runner.answer(claude, QStringLiteral("-p"), FakeProcessRunner::exited(0, highUsage()));
        FakeNotificationSink sink;
        LimitsJob job(fixture.limits, fixture.paths(), SchedulePolicy::every(60'000));
        job.setNotifications(&sink, {.thresholds = {50, 60}, .notifyOnReset = false});
        job.start();
        QTRY_COMPARE(job.state().phase, Phase::Loaded);
        QVERIFY(!sink.notes.isEmpty());
        QVERIFY(sink.notes.first().title.startsWith(QStringLiteral("Claude Code: ")));
        const qsizetype sent = sink.notes.size();
        job.refresh();
        QTest::qWait(100);
        QCOMPARE(sink.notes.size(), sent);
        QVERIFY(!notificationstore::load(fixture.paths()).isEmpty());
    }

    // Review Focus 1: a restart does not notify the same cycle again.
    void aRestartDoesNotNotifyAgain() {
        Fixture fixture;
        fixture.runner.answer(claude, QStringLiteral("-p"), FakeProcessRunner::exited(0, highUsage()));
        FakeNotificationSink first;
        {
            LimitsJob job(fixture.limits, fixture.paths(), SchedulePolicy::every(60'000));
            job.setNotifications(&first, {.thresholds = {50, 60}, .notifyOnReset = false});
            job.start();
            QTRY_COMPARE(job.state().phase, Phase::Loaded);
        }
        QVERIFY(!first.notes.isEmpty());
        FakeNotificationSink second;
        LimitsJob job(fixture.limits, fixture.paths(), SchedulePolicy::every(60'000));
        job.setNotifications(&second, {.thresholds = {50, 60}, .notifyOnReset = false});
        job.start();
        QTRY_COMPARE(job.state().phase, Phase::Loaded);
        QTest::qWait(50);
        QVERIFY(second.notes.isEmpty());
    }

    // Review I-2, US-12: forgetting drops the cached limits and the notification cycles, then probes afresh.
    void forgetDataDropsCacheAndCycles() {
        Fixture fixture;
        fixture.runner.answer(claude, QStringLiteral("-p"), FakeProcessRunner::exited(0, highUsage()));
        FakeNotificationSink sink;
        LimitsJob job(fixture.limits, fixture.paths(), SchedulePolicy::every(60'000));
        job.setNotifications(&sink, {.thresholds = {50, 60}, .notifyOnReset = false});
        job.start();
        QTRY_COMPARE(job.state().phase, Phase::Loaded);
        QVERIFY(!notificationstore::load(fixture.paths()).isEmpty());
        const qsizetype probes = fixture.probes();
        job.forgetData();
        QVERIFY(!job.state().snapshot());
        QVERIFY(!limitscache::load(fixture.paths(), QStringLiteral("claude-code")));
        QVERIFY(!notificationstore::load(fixture.paths()).contains(QStringLiteral("claude-code")));
        QTRY_VERIFY(fixture.probes() > probes);
    }
};

QTEST_GUILESS_MAIN(TestLimitsJob)
#include "tst_limitsjob.moc"
