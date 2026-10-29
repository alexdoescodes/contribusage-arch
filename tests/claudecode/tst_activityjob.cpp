#include "claudecode/activityjob.h"
#include "support/fakeprocessrunner.h"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QTest>

#include <unistd.h>

using namespace contribusage;
using namespace contribusage::claudecode;

namespace {

const QString shell = QStringLiteral("/bin/sh");
using Phase = SourceState<ActivityReport>::Phase;

QByteArray entry(const QDateTime &timestamp, const char *message, int input) {
    return R"({"type":"assistant","timestamp":")" + timestamp.toUTC().toString(Qt::ISODateWithMs).toUtf8() +
           R"(","sessionId":"s1","requestId":"r","message":{"id":")" + QByteArray(message) +
           R"(","model":"opus","usage":{"input_tokens":)" + QByteArray::number(input) + "}}}\n";
}

void write(const QString &path, const QByteArray &bytes, QIODevice::OpenMode mode = QIODevice::WriteOnly) {
    QVERIFY(QDir().mkpath(QFileInfo(path).absolutePath()));
    QFile file(path);
    QVERIFY(file.open(mode));
    file.write(bytes);
}

struct Fixture {
    QTemporaryDir temporary;
    FakeProcessRunner runner;

    QString home() const { return temporary.filePath(QStringLiteral("home")); }
    QString configDir() const { return temporary.filePath(QStringLiteral("cfg")); }
    QString transcript() const { return configDir() + QStringLiteral("/projects/-code/s.jsonl"); }
    AppPaths paths() const { return {.root = temporary.filePath(QStringLiteral("data"))}; }

    Fixture() { runner.answer(shell, QStringLiteral("-lc"), FakeProcessRunner::exited(0, configDir() + u'\n')); }

    std::unique_ptr<ActivityJob> job() {
        return std::make_unique<ActivityJob>(runner, home(), shell, paths().root + QStringLiteral("/probe"), paths(),
                                             50);
    }
};

qint64 inputOn(const ActivityJob &job, QDate day) {
    const auto snapshot = job.state().snapshot();
    if (!snapshot) {
        return -1;
    }
    for (const ActivityDay &item : snapshot->value.days) {
        if (item.day == day) {
            return item.tokens.input;
        }
    }
    return 0;
}

} // namespace

// SPEC §8.3, FR-22 to FR-29; ported from TranscriptActivityTests.swift.
class TestActivityJob : public QObject {
    Q_OBJECT

private Q_SLOTS:
    // FR-22: CLAUDE_CONFIG_DIR from the login shell is a root.
    void reportsTheConfigDirTranscripts() {
        Fixture fixture;
        const QDateTime now = QDateTime::currentDateTime();
        write(fixture.transcript(), entry(now, "m1", 10));
        auto job = fixture.job();
        job->start();
        QTRY_COMPARE(job->state().phase, Phase::Loaded);
        QCOMPARE(inputOn(*job, now.date()), qint64(10));
        QCOMPARE(job->state().snapshot()->origin, origins::activity());
        QCOMPARE(fixture.runner.requests.at(0).arguments.at(0), QStringLiteral("-lc"));
    }

    void withoutTranscriptsThereIsNoLocalData() {
        Fixture fixture;
        auto job = fixture.job();
        job->start();
        QTRY_COMPARE(job->state().phase, Phase::NotConfigured);
        QCOMPARE(job->state().reason, NotConfiguredReason::NoLocalData);
    }

    void followsChanges() {
        Fixture fixture;
        const QDateTime now = QDateTime::currentDateTime();
        write(fixture.transcript(), entry(now, "m1", 10));
        auto job = fixture.job();
        job->start();
        QTRY_COMPARE(inputOn(*job, now.date()), qint64(10));
        write(fixture.transcript(), entry(now, "m2", 5), QIODevice::Append);
        QTRY_COMPARE(inputOn(*job, now.date()), qint64(15));
    }

    // Review Focus 2: a day older than 48 h is frozen and survives its transcript.
    void frozenDaysSurviveDeletedTranscripts() {
        Fixture fixture;
        const QDateTime old = QDateTime::currentDateTime().addDays(-10);
        write(fixture.transcript(), entry(old, "m1", 7));
        auto job = fixture.job();
        job->start();
        QTRY_COMPARE(inputOn(*job, old.date()), qint64(7));
        QVERIFY(QFile::remove(fixture.transcript()));
        job->rescan();
        QTest::qWait(200);
        QCOMPARE(inputOn(*job, old.date()), qint64(7));
        QVERIFY(QFile::exists(fixture.paths().providerFolder(QStringLiteral("claude-code")) +
                              QStringLiteral("/history.json")));
    }

    // Review Focus 4: a damaged history stays on disk; the live days still show.
    void damagedHistoryShowsLiveDays() {
        Fixture fixture;
        const QString history =
            fixture.paths().providerFolder(QStringLiteral("claude-code")) + QStringLiteral("/history.json");
        write(history, "damaged");
        const QDateTime old = QDateTime::currentDateTime().addDays(-10);
        write(fixture.transcript(), entry(old, "m1", 7));
        auto job = fixture.job();
        job->start();
        QTRY_COMPARE(inputOn(*job, old.date()), qint64(7));
        QFile file(history);
        QVERIFY(file.open(QIODevice::ReadOnly));
        QCOMPARE(file.readAll(), QByteArray("damaged"));
    }

    // Review Focus 5: destroying the job while a scan runs neither crashes nor waits for the whole tree.
    void destroyedDuringAScan() {
        Fixture fixture;
        const QDateTime now = QDateTime::currentDateTime();
        for (int index = 0; index < 200; ++index) {
            write(fixture.configDir() + QStringLiteral("/projects/-code/s") + QString::number(index) +
                      QStringLiteral(".jsonl"),
                  entry(now, "m", index).repeated(200));
        }
        auto job = fixture.job();
        job->start();
        QTRY_VERIFY(!fixture.runner.requests.isEmpty());
        QTest::qWait(20);
        job.reset();
        QVERIFY(true);
    }

    // A day is not frozen from a scan that could not read every transcript: it would stay short forever.
    void anUnreadableTranscriptKeepsItsDayUnfrozen() {
        if (geteuid() == 0) {
            QSKIP("root reads files whatever their permissions");
        }
        Fixture fixture;
        const QDateTime old = QDateTime::currentDateTime().addDays(-10);
        const QString second = fixture.configDir() + QStringLiteral("/projects/-code/t.jsonl");
        write(fixture.transcript(), entry(old, "m1", 7));
        write(second, entry(old, "m2", 5));
        QVERIFY(QFile::setPermissions(second, QFileDevice::Permissions()));
        auto job = fixture.job();
        job->start();
        QTRY_COMPARE(inputOn(*job, old.date()), qint64(7));
        QVERIFY(QFile::setPermissions(second, QFileDevice::ReadOwner | QFileDevice::WriteOwner));
        job->rescan();
        QTRY_COMPARE(inputOn(*job, old.date()), qint64(12));
    }

    // Review I-2, US-12: the history is deleted on the worker's own thread; days whose transcripts are gone go with it.
    void deleteHistoryForgetsFrozenDays() {
        Fixture fixture;
        const QDateTime old = QDateTime::currentDateTime().addDays(-10);
        write(fixture.transcript(), entry(old, "m1", 7));
        auto job = fixture.job();
        job->start();
        QTRY_COMPARE(inputOn(*job, old.date()), qint64(7));
        const QString history =
            fixture.paths().providerFolder(QStringLiteral("claude-code")) + QStringLiteral("/history.json");
        QTRY_VERIFY(QFile::exists(history));
        QVERIFY(QFile::remove(fixture.transcript()));
        job->deleteHistory();
        QTRY_VERIFY(inputOn(*job, old.date()) <= 0); // gone; with no transcript left there is no local data at all
        QVERIFY(!QFile::exists(history));
    }

    // Review Focus 4, FR-46: inactive, the job neither watches nor scans; active again, it catches up.
    void hiddenActivityStopsTheWatcher() {
        Fixture fixture;
        const QDateTime now = QDateTime::currentDateTime();
        write(fixture.transcript(), entry(now, "m1", 10));
        auto job = fixture.job();
        job->start();
        QTRY_COMPARE(inputOn(*job, now.date()), qint64(10));
        job->setActive(false);
        write(fixture.transcript(), entry(now, "m2", 5), QIODevice::Append);
        QTest::qWait(300);
        QCOMPARE(inputOn(*job, now.date()), qint64(10));
        job->setActive(true);
        QTRY_COMPARE(inputOn(*job, now.date()), qint64(15));
    }
};

QTEST_GUILESS_MAIN(TestActivityJob)
#include "tst_activityjob.moc"
