#include "claudecode/claudecodelimits.h"

#include "claudecode/usageparser.h"
#include "core/jsonstore.h"

#include <QRegularExpression>

namespace contribusage::claudecode {

ClaudeCodeLimits::ClaudeCodeLimits(ProcessRunning &runner, ClaudeLocator &locator, AppPaths paths,
                                   std::function<QDateTime()> now)
    : m_runner(runner), m_locator(locator), m_paths(std::move(paths)), m_now(std::move(now)) {}

QString ClaudeCodeLimits::probeFolder() const {
    return m_paths.providerFolder(providerId()) + QStringLiteral("/probe");
}

void ClaudeCodeLimits::setOverride(const QString &path) {
    if (path != m_override) {
        m_override = path;
        m_located.reset();
    }
}

void ClaudeCodeLimits::fetch(QObject *context, std::function<void(LimitsOutcome)> done) {
    if (!jsonstore::createFolder(probeFolder())) {
        done(std::unexpected(
            SourceError{.kind = SourceError::Kind::Io, .text = QStringLiteral("cannot create ") + probeFolder()}));
        return;
    }
    probe(1, context, std::move(done));
}

void ClaudeCodeLimits::probe(int attempt, QObject *context, std::function<void(LimitsOutcome)> done) {
    if (!m_located) {
        m_locator.locate(m_override, context,
                         [this, attempt, context, done = std::move(done)](std::optional<FoundClaude> found) mutable {
                             if (!found) {
                                 done(std::unexpected(SourceError{.kind = SourceError::Kind::ToolNotFound}));
                                 return;
                             }
                             m_located = std::move(found);
                             probe(attempt, context, std::move(done));
                         });
        return;
    }
    const ProcessRequest request{
        .executable = m_located->executable,
        .arguments = {QStringLiteral("-p"), QStringLiteral("/usage"), QStringLiteral("--no-session-persistence")},
        .workingDirectory = probeFolder(),
        .environment = m_located->environment,
        .timeoutMs = 30'000};
    m_runner.run(request, context, [this, attempt, context, done = std::move(done)](ProcessOutcome outcome) mutable {
        if (!outcome && outcome.error().kind == SourceError::Kind::Io) {
            // A launch failure: the path stopped working, for example after an update moved it (FR-6).
            m_located.reset();
            if (attempt == 1) {
                probe(2, context, std::move(done));
            } else {
                done(std::unexpected(SourceError{.kind = SourceError::Kind::ToolNotFound}));
            }
            return;
        }
        if (!outcome) {
            done(std::unexpected(outcome.error()));
            return;
        }
        done(reportFrom(*outcome));
    });
}

LimitsOutcome ClaudeCodeLimits::reportFrom(const ProcessResult &result) const {
    if (result.exitCode != 0) {
        // "login", "log in", "logged in"
        static const QRegularExpression login(QStringLiteral("\\blog(ged)? ?in\\b"),
                                              QRegularExpression::CaseInsensitiveOption);
        if (login.match(result.stdoutText + u'\n' + result.stderrText).hasMatch()) {
            return std::unexpected(SourceError{.kind = SourceError::Kind::NotLoggedIn});
        }
        return std::unexpected(SourceError{
            .kind = SourceError::Kind::ProcessFailed, .text = result.stderrText.right(500), .code = result.exitCode});
    }
    const LimitsReport report = parseUsage(result.stdoutText, m_now());
    if (report.billingNote && !report.billingNote->contains(QStringLiteral("subscription"), Qt::CaseInsensitive)) {
        return std::unexpected(
            SourceError{.kind = SourceError::Kind::UnsupportedPlan, .text = *report.billingNote}); // P-10
    }
    if (report.windows.isEmpty()) {
        return std::unexpected(SourceError{.kind = SourceError::Kind::Unparseable, .text = result.stdoutText}); // FR-9
    }
    return report;
}

} // namespace contribusage::claudecode
