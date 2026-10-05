#include "core/sourceschedule.h"

#include <algorithm>
#include <limits>

namespace contribusage {

void ScheduleConditionsSource::setConditions(const ScheduleConditions &conditions) {
    if (conditions == m_conditions) {
        return;
    }
    m_conditions = conditions;
    Q_EMIT changed();
}

SourceSchedule::SourceSchedule(SchedulePolicy policy, ScheduleConditionsSource *conditions, QObject *parent)
    : QObject(parent), m_policy(policy), m_conditions(conditions) {
    m_timer.setSingleShot(true);
    connect(&m_timer, &QTimer::timeout, this, &SourceSchedule::fire);
    m_deadline.setSingleShot(true);
    m_deadline.setInterval(120'000); // longer than the probe's 30 s and the network's 30 s timeouts
    connect(&m_deadline, &QTimer::timeout, this, [this] { finished(false, true, m_run); });
    if (conditions) {
        m_wasOffline = !conditions->conditions().online;
        connect(conditions, &ScheduleConditionsSource::changed, this, [this] {
            // SPEC §12 rule 2: a source that waited for the network runs within its manual floor once it is back.
            const bool offline = !m_conditions->conditions().online;
            if (m_wasOffline && !offline && m_policy.needsNetwork) {
                m_manual = true;
            }
            m_wasOffline = offline;
            arm();
        });
    }
}

std::optional<QDateTime> SourceSchedule::lastRun() const {
    if (m_lastAttempt && (!m_lastSuccess || *m_lastAttempt > *m_lastSuccess)) {
        return m_lastAttempt;
    }
    return m_lastSuccess;
}

void SourceSchedule::start() {
    m_started = true;
    arm();
}

void SourceSchedule::finished(bool success, bool backoff, quint64 run) {
    if (!m_running || (run != 0 && run != m_run)) {
        return; // an answer for a run that already ended
    }
    m_deadline.stop();
    m_running = false;
    const QDateTime now = QDateTime::currentDateTimeUtc();
    if (success) {
        m_lastSuccess = now;
        m_failures = 0;
        m_hold.reset();
    } else if (backoff) {
        ++m_failures;
    }
    const std::optional<QDateTime> last = lastRun();
    m_triggers.removeIf([&last](const QDateTime &trigger) { return last && trigger <= *last; });
    arm();
}

void SourceSchedule::trigger(const QDateTime &at) {
    m_triggers.append(at);
    arm();
}

bool SourceSchedule::triggerIfDue() {
    const QDateTime now = QDateTime::currentDateTimeUtc();
    const std::optional<QDateTime> last = lastRun();
    if (m_running || (last && last->msecsTo(now) < m_policy.minimumMs)) {
        return false;
    }
    trigger(now);
    return true;
}

void SourceSchedule::setIntervalOverride(std::optional<qint64> ms) {
    m_intervalOverride = ms;
    arm();
}

void SourceSchedule::setRunDeadline(int ms) {
    m_deadline.setInterval(ms);
}

void SourceSchedule::requestManual() {
    m_manual = true;
    arm();
}

void SourceSchedule::holdUntil(const QDateTime &until) {
    m_hold = until;
    arm();
}

void SourceSchedule::setPaused(bool paused) {
    m_paused = paused;
    arm();
}

void SourceSchedule::setPolicy(SchedulePolicy policy) {
    m_policy = policy;
    arm();
}

void SourceSchedule::reset() {
    m_lastSuccess.reset();
    m_lastAttempt.reset();
    m_hold.reset();
    m_triggers.clear();
    m_failures = 0;
    m_running = false;
    m_deadline.stop();
    m_manual = false;
    m_paused = false;
    m_intervalOverride.reset();
    arm();
}

void SourceSchedule::arm() {
    m_timer.stop();
    if (!m_started || m_running || (m_paused && !m_manual)) {
        return;
    }
    const QDateTime now = QDateTime::currentDateTimeUtc();
    const ScheduleConditions conditions = m_conditions ? m_conditions->conditions() : ScheduleConditions{};
    SchedulePolicy policy = m_policy;
    if (m_intervalOverride) {
        policy.defaultMs = policy.minimumMs = policy.maximumMs = *m_intervalOverride;
    }
    std::optional<QDateTime> next =
        nextRun(policy, m_lastSuccess, m_lastAttempt, m_failures, now, conditions, m_manual, m_triggers);
    if (!next) {
        return; // asleep or offline: a change of the conditions arms again
    }
    if (m_hold && *m_hold > *next) {
        next = m_hold;
    }
    const qint64 delay = std::clamp<qint64>(now.msecsTo(*next), 0, std::numeric_limits<int>::max());
    m_timer.start(int(delay));
}

void SourceSchedule::fire() {
    m_running = true;
    ++m_run;
    m_deadline.start();
    m_manual = false;
    m_lastAttempt = QDateTime::currentDateTimeUtc();
    Q_EMIT due();
}

} // namespace contribusage
