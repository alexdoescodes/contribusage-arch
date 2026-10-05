#include "claudecode/limitsjob.h"

#include "claudecode/usageparser.h"
#include "core/limitscache.h"

namespace contribusage::claudecode {

LimitsJob::LimitsJob(ClaudeCodeLimits &limits, AppPaths paths, SchedulePolicy policy,
                     ScheduleConditionsSource *conditions, QObject *parent)
    : QObject(parent), m_limits(limits), m_paths(std::move(paths)), m_schedule(policy, conditions) {
    connect(&m_schedule, &SourceSchedule::due, this, &LimitsJob::run);
}

SchedulePolicy LimitsJob::defaultPolicy() {
    return {.defaultMs = 5 * 60'000,
            .minimumMs = 60'000,
            .maximumMs = 60 * 60'000,
            .staleAfterMs = 30 * 60'000,
            .manualFloorMs = 30'000,
            .needsNetwork = true};
}

void LimitsJob::start() {
    m_state = SourceState<LimitsReport>::loading(limitscache::load(m_paths, providerId()));
    Q_EMIT stateChanged();
    m_schedule.start();
}

void LimitsJob::refresh() {
    m_schedule.requestManual();
}

void LimitsJob::popupOpened() {
    const auto snapshot = m_state.snapshot();
    if (!snapshot || snapshot->fetchedAt.msecsTo(QDateTime::currentDateTimeUtc()) > 5 * 60'000) {
        m_schedule.triggerIfDue();
    }
}

void LimitsJob::addResetTrigger(const QDateTime &at) {
    m_schedule.trigger(at);
}

void LimitsJob::run() {
    // `this` as context: a job deleted during a probe never gets its result (L-1a's guard).
    m_limits.fetch(this, [this](LimitsOutcome outcome) { finish(std::move(outcome)); });
}

void LimitsJob::finish(LimitsOutcome outcome) {
    const QDateTime now = QDateTime::currentDateTimeUtc();
    m_schedule.finished(outcome.has_value());
    if (outcome) {
        const Snapshot<LimitsReport> snapshot{
            .value = std::move(*outcome), .fetchedAt = now, .origin = origins::poll()};
        limitscache::save(m_paths, snapshot);
        m_state = SourceState<LimitsReport>::loaded(snapshot);
        for (const UsageWindow &window : snapshot.value.windows) {
            if (window.resetsAt && *window.resetsAt > now) {
                m_schedule.trigger(*window.resetsAt); // FR-11
            }
        }
    } else if (outcome.error().kind == SourceError::Kind::ToolNotFound) {
        m_state = SourceState<LimitsReport>::notConfigured(NotConfiguredReason::ToolNotInstalled);
    } else if (outcome.error().kind == SourceError::Kind::UnsupportedPlan) {
        m_state = SourceState<LimitsReport>::notConfigured(NotConfiguredReason::UnsupportedPlan, outcome.error().text);
        m_schedule.holdUntil(now.addSecs(6 * 3600)); // SPEC §13: re-check every 6 h (ADR-018)
    } else {
        m_state = SourceState<LimitsReport>::failed(outcome.error(), m_state.snapshot());
    }
    Q_EMIT stateChanged();
}

} // namespace contribusage::claudecode
