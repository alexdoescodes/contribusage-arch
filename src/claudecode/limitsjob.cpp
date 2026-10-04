#include "claudecode/limitsjob.h"

#include "claudecode/usageparser.h"
#include "core/limitscache.h"

namespace contribusage::claudecode {

LimitsJob::LimitsJob(ClaudeCodeLimits &limits, AppPaths paths, int intervalMs, QObject *parent)
    : QObject(parent), m_limits(limits), m_paths(std::move(paths)) {
    m_timer.setInterval(intervalMs);
    connect(&m_timer, &QTimer::timeout, this, &LimitsJob::refresh);
}

void LimitsJob::start() {
    m_state = SourceState<LimitsReport>::loading(limitscache::load(m_paths, providerId()));
    Q_EMIT stateChanged();
    refresh();
    m_timer.start();
}

void LimitsJob::refresh() {
    if (m_fetching) {
        return;
    }
    m_fetching = true;
    // `this` as context: a job deleted during a probe never gets its result (L-1a's guard).
    m_limits.fetch(this, [this](LimitsOutcome outcome) { finish(std::move(outcome)); });
}

void LimitsJob::finish(LimitsOutcome outcome) {
    m_fetching = false;
    if (outcome) {
        const Snapshot<LimitsReport> snapshot{
            .value = std::move(*outcome), .fetchedAt = QDateTime::currentDateTimeUtc(), .origin = origins::poll()};
        limitscache::save(m_paths, snapshot);
        m_state = SourceState<LimitsReport>::loaded(snapshot);
    } else if (outcome.error().kind == SourceError::Kind::ToolNotFound) {
        m_state = SourceState<LimitsReport>::notConfigured(NotConfiguredReason::ToolNotInstalled);
    } else if (outcome.error().kind == SourceError::Kind::UnsupportedPlan) {
        m_state = SourceState<LimitsReport>::notConfigured(NotConfiguredReason::UnsupportedPlan, outcome.error().text);
    } else {
        m_state = SourceState<LimitsReport>::failed(outcome.error(), m_state.snapshot());
    }
    Q_EMIT stateChanged();
}

} // namespace contribusage::claudecode
