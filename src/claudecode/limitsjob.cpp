#include "claudecode/limitsjob.h"

#include "core/notificationstore.h"

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
    m_sent = notificationstore::load(m_paths);
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

void LimitsJob::setNotifications(NotificationSink *sink, NotificationSettings settings) {
    m_sink = sink;
    m_notificationSettings = std::move(settings);
}

void LimitsJob::setInterval(int minutes) {
    SchedulePolicy policy = m_schedule.policy();
    policy.defaultMs = qint64(minutes) * 60'000;
    m_schedule.setPolicy(policy);
}

void LimitsJob::addResetTrigger(const QDateTime &at) {
    m_schedule.trigger(at);
}

void LimitsJob::run() {
    // `this` as context: a job deleted during a probe never gets its result (L-1a's guard).
    m_limits.fetch(this,
                   [this, run = m_schedule.currentRun()](LimitsOutcome outcome) { finish(std::move(outcome), run); });
}

void LimitsJob::finish(LimitsOutcome outcome, quint64 run) {
    const QDateTime now = QDateTime::currentDateTimeUtc();
    // SPEC §13: an unsupported plan is an answer, re-checked every 6 h (ADR-018); no network is no reason to back off.
    const bool unsupported = !outcome && outcome.error().kind == SourceError::Kind::UnsupportedPlan;
    const bool offline = !outcome && outcome.error().kind == SourceError::Kind::Offline;
    m_schedule.finished(outcome.has_value() || unsupported, !offline, run);
    m_schedule.setIntervalOverride(unsupported ? std::optional<qint64>(6 * 3600'000) : std::nullopt);
    if (outcome) {
        const Snapshot<LimitsReport> snapshot{
            .value = std::move(*outcome), .fetchedAt = now, .origin = origins::poll()};
        limitscache::save(m_paths, snapshot);
        m_state = SourceState<LimitsReport>::loaded(snapshot);
        if (m_sink) {
            const NotificationsSent before = m_sent;
            const QList<NotificationNote> notes = planNotifications(
                snapshot.value, QStringLiteral("Claude Code"), [](const QString &label) { return label; },
                m_notificationSettings, m_sent, now);
            for (const NotificationNote &note : notes) {
                m_sink->notify(note);
            }
            if (m_sent != before) {
                notificationstore::save(m_paths, m_sent);
            }
        }
        for (const UsageWindow &window : snapshot.value.windows) {
            if (window.resetsAt && *window.resetsAt > now) {
                m_schedule.trigger(*window.resetsAt); // FR-11
            }
        }
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
