#include "claudecode/claudelocator.h"
#include "support/fakeprocessrunner.h"

#include <QEventLoop>
#include <QTest>
#include <QTimer>

using namespace contribusage;
using namespace contribusage::claudecode;

namespace {

const QString home = QStringLiteral("/home/octocat");
const QString shell = QStringLiteral("/usr/bin/zsh");
const QString shellPath = QStringLiteral("/home/octocat/.local/bin:/usr/bin");

std::optional<FoundClaude> locate(FakeProcessRunner &runner, const QString &override = {}) {
    std::optional<std::optional<FoundClaude>> result;
    QEventLoop loop;
    ClaudeLocator locator(runner, home, shell);
    locator.locate(override, nullptr, [&](std::optional<FoundClaude> found) {
        result = std::move(found);
        loop.quit();
    });
    if (!result) {
        QTimer::singleShot(5'000, &loop, &QEventLoop::quit);
        loop.exec();
    }
    return result.value_or(std::nullopt);
}

ProcessOutcome version(const char *text) {
    return FakeProcessRunner::exited(0, QString::fromLatin1(text) + u'\n');
}

} // namespace

// FR-6, ported from ClaudeLocatorTests.swift with Linux locations.
class TestClaudeLocator : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void overrideWinsWhenValid() {
        FakeProcessRunner runner;
        runner.answer(shell, {}, FakeProcessRunner::exited(0, QStringLiteral("/usr/bin/claude\n") + shellPath + u'\n'));
        runner.answer(QStringLiteral("/custom/claude"), QStringLiteral("--version"), version("2.1.289 (Claude Code)"));
        const auto found = locate(runner, QStringLiteral("/custom/claude"));
        QVERIFY(found);
        QCOMPARE(found->executable, QStringLiteral("/custom/claude"));
        QCOMPARE(found->version, QStringLiteral("2.1.289 (Claude Code)"));
        QCOMPARE(runner.requests.at(1).arguments, QStringList{QStringLiteral("--version")});
        QCOMPARE(runner.requests.at(1).timeoutMs, 10'000);
    }

    void usesLoginShellResultAndPath() {
        FakeProcessRunner runner;
        runner.answer(
            shell, {},
            FakeProcessRunner::exited(0, QStringLiteral("/home/octocat/.local/bin/claude\n") + shellPath + u'\n'));
        runner.answer(QStringLiteral("/home/octocat/.local/bin/claude"), QStringLiteral("--version"),
                      version("2.1.289 (Claude Code)"));
        const auto found = locate(runner);
        QVERIFY(found);
        QCOMPARE(found->executable, QStringLiteral("/home/octocat/.local/bin/claude"));
        QCOMPARE(runner.requests.at(0).arguments,
                 (QStringList{QStringLiteral("-lc"), QStringLiteral("command -v claude; printf '%s\\n' \"$PATH\"")}));
        QCOMPARE(runner.requests.at(0).timeoutMs, 10'000);
        QVERIFY(found->environment);
        QCOMPARE(found->environment->value(QStringLiteral("PATH")), shellPath);
        QCOMPARE(runner.requests.at(1).environment->value(QStringLiteral("PATH")), shellPath);
    }

    // Profile scripts may print before; an alias or a miss is not a path.
    void toleratesProfileOutputAndMissingClaude() {
        FakeProcessRunner runner;
        runner.answer(shell, {},
                      FakeProcessRunner::exited(0, QStringLiteral("Welcome!\nalias claude=foo\n") + shellPath + u'\n'));
        runner.answer(QStringLiteral("/usr/local/bin/claude"), QStringLiteral("--version"),
                      version("2.0.0 (Claude Code)"));
        const auto found = locate(runner);
        QVERIFY(found);
        QCOMPARE(found->executable, QStringLiteral("/usr/local/bin/claude"));
        QCOMPARE(found->environment->value(QStringLiteral("PATH")), shellPath);
    }

    void fallsBackToKnownLocationsInOrder() {
        FakeProcessRunner runner;
        runner.answer(shell, {}, FakeProcessRunner::exited(1, {}));
        runner.answer(QStringLiteral("/usr/bin/claude"), QStringLiteral("--version"), version("2.0.0 (Claude Code)"));
        const auto found = locate(runner, QStringLiteral("/nowhere/claude"));
        QVERIFY(found);
        QCOMPARE(found->executable, QStringLiteral("/usr/bin/claude"));
        QVERIFY(!found->environment); // a failing shell inherits the environment
        QStringList tried;
        for (const auto &request : runner.requests) {
            tried.append(request.executable);
        }
        QCOMPARE(tried, (QStringList{shell, QStringLiteral("/nowhere/claude"),
                                     QStringLiteral("/home/octocat/.local/bin/claude"),
                                     QStringLiteral("/home/octocat/.npm-global/bin/claude"),
                                     QStringLiteral("/usr/local/bin/claude"), QStringLiteral("/usr/bin/claude")}));
    }

    void skipsCandidatesWhoseVersionFails() {
        FakeProcessRunner runner;
        runner.answer(shell, {}, FakeProcessRunner::exited(1, {}));
        runner.answer(QStringLiteral("/home/octocat/.local/bin/claude"), QStringLiteral("--version"),
                      FakeProcessRunner::exited(127, {}));
        runner.answer(QStringLiteral("/usr/local/bin/claude"), QStringLiteral("--version"),
                      version("2.0.0 (Claude Code)"));
        const auto found = locate(runner);
        QVERIFY(found);
        QCOMPARE(found->executable, QStringLiteral("/usr/local/bin/claude"));
    }

    void returnsNothingWhenNothingIsValid() {
        FakeProcessRunner runner;
        QVERIFY(!locate(runner));
    }
};

QTEST_GUILESS_MAIN(TestClaudeLocator)
#include "tst_claudelocator.moc"
