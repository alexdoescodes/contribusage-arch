#include "claudecode/claudecodelimits.h"
#include "support/fakeprocessrunner.h"

#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QTemporaryDir>
#include <QTest>
#include <QTimer>

using namespace contribusage;
using namespace contribusage::claudecode;

namespace {

const QString shell = QStringLiteral("/usr/bin/zsh");
const QString claude = QStringLiteral("/home/octocat/.local/bin/claude");

QString fixture(const char *name) {
    QFile file(QStringLiteral(CONTRIBUSAGE_FIXTURES "/usage/") + QString::fromLatin1(name) + QStringLiteral(".txt"));
    return file.open(QIODevice::ReadOnly) ? QString::fromUtf8(file.readAll()) : QString();
}

// Finds claude through the login shell.
void scriptLocator(FakeProcessRunner &runner) {
    runner.answer(shell, {}, FakeProcessRunner::exited(0, claude + QStringLiteral("\n/usr/bin\n")));
    runner.answer(claude, QStringLiteral("--version"),
                  FakeProcessRunner::exited(0, QStringLiteral("2.1.289 (Claude Code)\n")));
}

LimitsOutcome fetch(ClaudeCodeLimits &limits) {
    std::optional<LimitsOutcome> outcome;
    QEventLoop loop;
    limits.fetch(nullptr, [&](LimitsOutcome result) {
        outcome = std::move(result);
        loop.quit();
    });
    if (!outcome) {
        QTimer::singleShot(5'000, &loop, &QEventLoop::quit);
        loop.exec();
    }
    return outcome.value_or(
        std::unexpected(SourceError{.kind = SourceError::Kind::Io, .text = QStringLiteral("no answer")}));
}

} // namespace

// FR-7, P-10, FR-9 and SPEC §13, ported from ClaudeCodeProviderTests.swift.
class TestClaudeCodeLimits : public QObject {
    Q_OBJECT

private Q_SLOTS:
    // The hard rule: exactly these arguments, in the empty probe folder, 30 s.
    void probesWithTheExactArgumentsInTheProbeFolder() {
        QTemporaryDir temporary;
        FakeProcessRunner runner;
        scriptLocator(runner);
        runner.answer(claude, QStringLiteral("-p"), FakeProcessRunner::exited(0, fixture("subscription-basic")));
        ClaudeLocator locator(runner, QStringLiteral("/home/octocat"), shell);
        ClaudeCodeLimits limits(runner, locator, AppPaths{.root = temporary.path()},
                                [] { return QDateTime(QDate(2026, 9, 27), QTime(20, 0), QTimeZone::UTC); });

        const LimitsOutcome outcome = fetch(limits);

        QVERIFY(outcome);
        QCOMPARE(outcome->windows.size(), 2);
        const ProcessRequest &probe = runner.requests.last();
        QCOMPARE(probe.executable, claude);
        QCOMPARE(probe.arguments, (QStringList{QStringLiteral("-p"), QStringLiteral("/usage"),
                                               QStringLiteral("--no-session-persistence")}));
        QCOMPARE(probe.workingDirectory, temporary.path() + QStringLiteral("/providers/claude-code/probe"));
        QCOMPARE(limits.probeFolder(), probe.workingDirectory);
        QVERIFY(QDir(probe.workingDirectory).exists());
        QVERIFY(QDir(probe.workingDirectory).isEmpty());
        QCOMPARE(probe.timeoutMs, 30'000);
        QCOMPARE(probe.environment->value(QStringLiteral("PATH")), QStringLiteral("/usr/bin"));
    }

    void mapsFailures_data() {
        QTest::addColumn<int>("exitCode");
        QTest::addColumn<QString>("stdoutText");
        QTest::addColumn<QString>("stderrText");
        QTest::addColumn<int>("kind");
        QTest::addColumn<QString>("text");
        const auto kind = [](SourceError::Kind value) { return int(value); };
        QTest::newRow("not logged in") << 1 << QString() << QStringLiteral("Please log in with /login")
                                       << kind(SourceError::Kind::NotLoggedIn) << QString();
        QTest::newRow("process failed") << 2 << QString() << QStringLiteral("boom")
                                        << kind(SourceError::Kind::ProcessFailed) << QStringLiteral("boom");
        QTest::newRow("API key billing") << 0 << fixture("not-subscription") << QString()
                                         << kind(SourceError::Kind::UnsupportedPlan)
                                         << QStringLiteral("Total cost:            $0.0000");
        QTest::newRow("no windows") << 0 << QStringLiteral("You are currently using your subscription\nNothing else\n")
                                    << QString() << kind(SourceError::Kind::Unparseable)
                                    << QStringLiteral("You are currently using your subscription\nNothing else\n");
    }
    void mapsFailures() {
        QFETCH(int, exitCode);
        QFETCH(QString, stdoutText);
        QFETCH(QString, stderrText);
        QFETCH(int, kind);
        QFETCH(QString, text);
        QTemporaryDir temporary;
        FakeProcessRunner runner;
        scriptLocator(runner);
        runner.answer(claude, QStringLiteral("-p"), FakeProcessRunner::exited(exitCode, stdoutText, stderrText));
        ClaudeLocator locator(runner, QStringLiteral("/home/octocat"), shell);
        ClaudeCodeLimits limits(runner, locator, AppPaths{.root = temporary.path()});

        const LimitsOutcome outcome = fetch(limits);

        QVERIFY(!outcome);
        QCOMPARE(int(outcome.error().kind), kind);
        QCOMPARE(outcome.error().text, text);
        if (outcome.error().kind == SourceError::Kind::ProcessFailed) {
            QCOMPARE(outcome.error().code, exitCode);
        }
    }

    void timeoutStaysTimedOut() {
        QTemporaryDir temporary;
        FakeProcessRunner runner;
        scriptLocator(runner);
        runner.answer(claude, QStringLiteral("-p"), std::unexpected(SourceError{.kind = SourceError::Kind::TimedOut}));
        ClaudeLocator locator(runner, QStringLiteral("/home/octocat"), shell);
        ClaudeCodeLimits limits(runner, locator, AppPaths{.root = temporary.path()});
        const LimitsOutcome outcome = fetch(limits);
        QVERIFY(!outcome);
        QCOMPARE(outcome.error().kind, SourceError::Kind::TimedOut);
    }

    void notFoundIsToolNotFound() {
        QTemporaryDir temporary;
        FakeProcessRunner runner; // no rules: the shell and every candidate fail
        ClaudeLocator locator(runner, QStringLiteral("/home/octocat"), shell);
        ClaudeCodeLimits limits(runner, locator, AppPaths{.root = temporary.path()});
        const LimitsOutcome outcome = fetch(limits);
        QVERIFY(!outcome);
        QCOMPARE(outcome.error().kind, SourceError::Kind::ToolNotFound);
    }

    // FR-6: a launch failure means the path stopped working; locate once more, then give up.
    void relocatesOnceWhenThePathStopsWorking() {
        QTemporaryDir temporary;
        FakeProcessRunner runner;
        scriptLocator(runner); // --version works, but no rule answers "-p": the probe fails to launch (Io)
        ClaudeLocator locator(runner, QStringLiteral("/home/octocat"), shell);
        ClaudeCodeLimits limits(runner, locator, AppPaths{.root = temporary.path()});

        const LimitsOutcome outcome = fetch(limits);

        QVERIFY(!outcome);
        QCOMPARE(outcome.error().kind, SourceError::Kind::ToolNotFound);
        qsizetype lookups = 0;
        qsizetype probes = 0;
        for (const auto &request : runner.requests) {
            lookups += request.executable == shell;
            probes += request.arguments.value(0) == u"-p";
        }
        QCOMPARE(lookups, 2);
        QCOMPARE(probes, 2);
    }

    void cachesTheLocatedPath() {
        QTemporaryDir temporary;
        FakeProcessRunner runner;
        scriptLocator(runner);
        runner.answer(claude, QStringLiteral("-p"), FakeProcessRunner::exited(0, fixture("subscription-basic")));
        ClaudeLocator locator(runner, QStringLiteral("/home/octocat"), shell);
        ClaudeCodeLimits limits(runner, locator, AppPaths{.root = temporary.path()});
        QVERIFY(fetch(limits));
        QVERIFY(fetch(limits));
        qsizetype lookups = 0;
        for (const auto &request : runner.requests) {
            lookups += request.executable == shell;
        }
        QCOMPARE(lookups, 1);
    }
};

QTEST_GUILESS_MAIN(TestClaudeCodeLimits)
#include "tst_claudecodelimits.moc"
