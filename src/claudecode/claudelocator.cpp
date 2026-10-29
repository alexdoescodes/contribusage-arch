#include "claudecode/claudelocator.h"

#include <QStringList>

namespace contribusage::claudecode {

ClaudeLocator::ClaudeLocator(ProcessRunning &runner, QString home, QString shell)
    : m_runner(runner), m_home(std::move(home)), m_shell(std::move(shell)) {}

void ClaudeLocator::locate(const QString &override, QObject *context,
                           std::function<void(std::optional<FoundClaude>)> done) {
    // One login shell run for both answers. PATH comes last so profile output only precedes it; the line before counts
    // as `claude` only when it is an absolute path (an alias prints `alias claude=…`, a miss prints nothing).
    const ProcessRequest lookup{
        .executable = m_shell,
        .arguments = {QStringLiteral("-lc"), QStringLiteral("command -v claude; printf '%s\\n' \"$PATH\"")},
        .workingDirectory = m_home,
        .environment = std::nullopt,
        .timeoutMs = 10'000};
    m_runner.run(lookup, context, [this, override, context, done = std::move(done)](ProcessOutcome outcome) mutable {
        QString fromShell;
        std::optional<QProcessEnvironment> environment;
        if (outcome && outcome->exitCode == 0) {
            const QStringList lines = outcome->stdoutText.split(u'\n', Qt::SkipEmptyParts);
            if (!lines.isEmpty()) {
                QProcessEnvironment withPath = QProcessEnvironment::systemEnvironment();
                withPath.insert(QStringLiteral("PATH"), lines.last());
                environment = withPath;
                if (lines.size() >= 2 && lines.at(lines.size() - 2).startsWith(u'/')) {
                    fromShell = lines.at(lines.size() - 2);
                }
            }
        }
        QStringList candidates;
        for (const QString &candidate :
             QStringList{override, fromShell, m_home + QStringLiteral("/.local/bin/claude"),
                         m_home + QStringLiteral("/.npm-global/bin/claude"), QStringLiteral("/usr/local/bin/claude"),
                         QStringLiteral("/usr/bin/claude")}) {
            if (!candidate.isEmpty() && !candidates.contains(candidate)) {
                candidates.append(candidate);
            }
        }
        tryCandidates(candidates, environment, context, std::move(done));
    });
}

void ClaudeLocator::tryCandidates(QStringList candidates, std::optional<QProcessEnvironment> environment,
                                  QObject *context, std::function<void(std::optional<FoundClaude>)> done) {
    if (candidates.isEmpty()) {
        done(std::nullopt);
        return;
    }
    const QString candidate = candidates.takeFirst();
    const ProcessRequest request{.executable = candidate,
                                 .arguments = {QStringLiteral("--version")},
                                 .workingDirectory = m_home,
                                 .environment = environment,
                                 .timeoutMs = 10'000};
    m_runner.run(
        request, context,
        [this, candidate, candidates, environment, context, done = std::move(done)](ProcessOutcome outcome) mutable {
            if (outcome && outcome->exitCode == 0) {
                done(FoundClaude{
                    .executable = candidate, .version = outcome->stdoutText.trimmed(), .environment = environment});
                return;
            }
            tryCandidates(candidates, environment, context, std::move(done));
        });
}

} // namespace contribusage::claudecode
