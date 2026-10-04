#pragma once

#include "claudecode/claudelocator.h"
#include "core/apppaths.h"
#include "core/reports.h"

#include <QDateTime>

#include <expected>
#include <functional>
#include <optional>

namespace contribusage::claudecode {

using LimitsOutcome = std::expected<LimitsReport, SourceError>;

// FR-7: the `/usage` probe with SPEC §8.1.1's exact invocation, and the outcome as SPEC §13 names it. Keeps the
// located `claude` until a launch fails (FR-6). The caller keeps this object alive while a fetch runs and passes a
// context that dies with it.
class ClaudeCodeLimits {
public:
    ClaudeCodeLimits(
        ProcessRunning &runner, ClaudeLocator &locator, AppPaths paths,
        std::function<QDateTime()> now = [] { return QDateTime::currentDateTime(); });

    QString probeFolder() const;
    void setOverride(const QString &path);
    void fetch(QObject *context, std::function<void(LimitsOutcome)> done);

private:
    void probe(int attempt, QObject *context, std::function<void(LimitsOutcome)> done);
    LimitsOutcome reportFrom(const ProcessResult &result) const;

    ProcessRunning &m_runner;
    ClaudeLocator &m_locator;
    AppPaths m_paths;
    std::function<QDateTime()> m_now;
    QString m_override;
    std::optional<FoundClaude> m_located;
};

} // namespace contribusage::claudecode
