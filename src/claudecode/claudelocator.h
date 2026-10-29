#pragma once

#include "core/process.h"

#include <QObject>
#include <QProcessEnvironment>
#include <QString>

#include <functional>
#include <optional>

namespace contribusage::claudecode {

struct FoundClaude {
    QString executable;
    QString version; // `claude --version`, trimmed
    // The widget's environment with PATH from the login shell (SPEC §8.1.1); nullopt inherits it. An npm install is a
    // `#!/usr/bin/env node` script, which fails without it when plasmashell's PATH lacks node.
    std::optional<QProcessEnvironment> environment;
};

// FR-6: finds the `claude` executable. Stateless; the caller caches the result and keeps the locator alive while a
// lookup runs.
class ClaudeLocator {
public:
    ClaudeLocator(ProcessRunning &runner, QString home, QString shell);

    // The first candidate in FR-6 order whose `--version` exits 0 within 10 s; an invalid override falls through.
    void locate(const QString &override, QObject *context, std::function<void(std::optional<FoundClaude>)> done);

private:
    void tryCandidates(QStringList candidates, std::optional<QProcessEnvironment> environment, QObject *context,
                       std::function<void(std::optional<FoundClaude>)> done);

    ProcessRunning &m_runner;
    QString m_home;
    QString m_shell;
};

} // namespace contribusage::claudecode
