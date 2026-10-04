#include "core/process.h"

#include <QDir>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QTemporaryDir>
#include <QTest>
#include <QTimer>

using namespace contribusage;

namespace {

ProcessRequest request(const QString &executable, const QStringList &arguments = {},
                       const QString &folder = QDir::tempPath(), int timeoutMs = 10'000) {
    return {.executable = executable,
            .arguments = arguments,
            .workingDirectory = folder,
            .environment = std::nullopt,
            .timeoutMs = timeoutMs};
}

// Runs `request` and spins the event loop until the callback arrives, or `waitMs` passes.
std::optional<ProcessOutcome> runAndWait(const ProcessRequest &request, int waitMs = 15'000) {
    std::optional<ProcessOutcome> outcome;
    QEventLoop loop;
    LiveProcessRunner().run(request, [&](ProcessOutcome result) {
        outcome = std::move(result);
        loop.quit();
    });
    if (!outcome) {
        QTimer::singleShot(waitMs, &loop, &QEventLoop::quit);
        loop.exec();
    }
    return outcome;
}

} // namespace

// SPEC §16.1 process tests: LiveProcessRunner against real system tools (FR-7, NFR-18).
class TestProcessRunner : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void capturesStdoutAndExitCode() {
        const auto echo = runAndWait(request(QStringLiteral("/bin/echo"), {QStringLiteral("hello")}));
        QVERIFY(echo && echo->has_value());
        QVERIFY(echo->value() ==
                (ProcessResult{.exitCode = 0, .stdoutText = QStringLiteral("hello\n"), .stderrText = QString()}));
        const auto failing = runAndWait(request(QStringLiteral("/usr/bin/false")));
        QVERIFY(failing && failing->has_value());
        QCOMPARE(failing->value().exitCode, 1);
    }

    void capturesStderrAndUsesTheWorkingDirectoryAndEnvironment() {
        QProcessEnvironment environment;
        environment.insert(QStringLiteral("GREETING"), QStringLiteral("hi"));
        ProcessRequest script =
            request(QStringLiteral("/bin/sh"), {QStringLiteral("-c"), QStringLiteral("pwd; echo \"$GREETING\" >&2")},
                    QStringLiteral("/usr"));
        script.environment = environment;
        const auto result = runAndWait(script);
        QVERIFY(result && result->has_value());
        QCOMPARE(result->value().stdoutText, QStringLiteral("/usr\n"));
        QCOMPARE(result->value().stderrText, QStringLiteral("hi\n"));
    }

    // Output larger than the pipe buffer on both streams deadlocks a runner that waits for exit before reading.
    void readsLargeOutputOnBothStreams() {
        const auto result =
            runAndWait(request(QStringLiteral("/bin/sh"),
                               {QStringLiteral("-c"), QStringLiteral("head -c 500000 /dev/zero | tr '\\0' a; "
                                                                     "head -c 500000 /dev/zero | tr '\\0' b >&2")}));
        QVERIFY(result && result->has_value());
        QCOMPARE(result->value().stdoutText.size(), qsizetype(500'000));
        QCOMPARE(result->value().stderrText.size(), qsizetype(500'000));
    }

    // `cat` waits forever on an inherited or open stdin.
    void stdinIsDevNull() {
        const auto result = runAndWait(request(QStringLiteral("/bin/cat"), {}, QDir::tempPath(), 5'000));
        QVERIFY(result && result->has_value());
        QCOMPARE(result->value().stdoutText, QString());
    }

    void timeoutFailsWithTimedOut() {
        QElapsedTimer clock;
        clock.start();
        const auto result =
            runAndWait(request(QStringLiteral("/bin/sleep"), {QStringLiteral("10")}, QDir::tempPath(), 200));
        QVERIFY(result && !result->has_value());
        QCOMPARE(result->error().kind, SourceError::Kind::TimedOut);
        QVERIFY(clock.elapsed() < 2'000);
    }

    void killsAProcessThatIgnoresSIGTERM() {
        QElapsedTimer clock;
        clock.start();
        const auto result = runAndWait(
            request(QStringLiteral("/bin/sh"),
                    {QStringLiteral("-c"), QStringLiteral("trap '' TERM; exec /bin/sleep 10")}, QDir::tempPath(), 200));
        QVERIFY(result && !result->has_value());
        QCOMPARE(result->error().kind, SourceError::Kind::TimedOut);
        QVERIFY(clock.elapsed() < 5'000); // 0.2 s timeout + 2 s grace, not 10 s
    }

    void missingExecutableFailsAndTheQueueMovesOn() {
        const auto missing = runAndWait(request(QStringLiteral("/no/such/tool")));
        QVERIFY(missing && !missing->has_value());
        QCOMPARE(missing->error().kind, SourceError::Kind::Io);
        const auto next = runAndWait(request(QStringLiteral("/bin/echo"), {QStringLiteral("next")}));
        QVERIFY(next && next->has_value());
        QCOMPARE(next->value().stdoutText, QStringLiteral("next\n"));
    }

    // NFR-18: `mkdir` is atomic, so a second process running at the same time fails to take the lock.
    void runsOneProcessAtATime() {
        QTemporaryDir folder;
        const ProcessRequest script = request(
            QStringLiteral("/bin/sh"),
            {QStringLiteral("-c"), QStringLiteral("mkdir lock || exit 7; sleep 0.2; rmdir lock")}, folder.path());
        QList<int> codes;
        QEventLoop loop;
        LiveProcessRunner runner;
        for (int index = 0; index < 3; ++index) {
            runner.run(script, [&](ProcessOutcome outcome) {
                codes.append(outcome ? outcome->exitCode : -1);
                if (codes.size() == 3) {
                    loop.quit();
                }
            });
        }
        QTimer::singleShot(15'000, &loop, &QEventLoop::quit);
        loop.exec();
        QCOMPARE(codes, (QList<int>{0, 0, 0}));
    }

    // A widget removed during a probe: its view model is gone, so the result must not reach it.
    void aDestroyedContextGetsNoCallbackAndTheQueueMovesOn() {
        auto *owner = new QObject;
        bool called = false;
        LiveProcessRunner().run(request(QStringLiteral("/bin/sleep"), {QStringLiteral("0.2")}), owner,
                                [&](ProcessOutcome) { called = true; });
        delete owner;
        const auto next = runAndWait(request(QStringLiteral("/bin/echo"), {QStringLiteral("after")}));
        QVERIFY(next && next->has_value());
        QCOMPARE(next->value().stdoutText, QStringLiteral("after\n"));
        QVERIFY(!called);
    }

    void aLiveContextGetsItsCallback() {
        QObject owner;
        std::optional<ProcessOutcome> outcome;
        QEventLoop loop;
        LiveProcessRunner().run(request(QStringLiteral("/bin/echo"), {QStringLiteral("kept")}), &owner,
                                [&](ProcessOutcome result) {
                                    outcome = std::move(result);
                                    loop.quit();
                                });
        QTimer::singleShot(15'000, &loop, &QEventLoop::quit);
        loop.exec();
        QVERIFY(outcome && outcome->has_value());
        QCOMPARE(outcome->value().stdoutText, QStringLiteral("kept\n"));
    }
};

QTEST_GUILESS_MAIN(TestProcessRunner)
#include "tst_processrunner.moc"
