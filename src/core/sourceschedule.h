#pragma once

#include "core/schedule.h"

#include <QObject>
#include <QPointer>
#include <QTimer>

namespace contribusage {

// The machine's conditions, shared by every source's schedule; the platform layer sets them (SPEC §12 rules 1 to 3).
class ScheduleConditionsSource : public QObject {
    Q_OBJECT

public:
    using QObject::QObject;

    const ScheduleConditions &conditions() const { return m_conditions; }
    void setConditions(const ScheduleConditions &conditions);

Q_SIGNALS:
    void changed();

private:
    ScheduleConditions m_conditions;
};

// One polled source's runs (SPEC §12): arms a single timer for nextRun() and emits `due()` when it fires. Between
// `due()` and `finished()` the source is running and nothing else starts it. Without a conditions source the machine
// counts as awake and online.
class SourceSchedule : public QObject {
    Q_OBJECT

public:
    explicit SourceSchedule(SchedulePolicy policy, ScheduleConditionsSource *conditions = nullptr,
                            QObject *parent = nullptr);

    const SchedulePolicy &policy() const { return m_policy; }
    // The later of the last success and the last attempt.
    std::optional<QDateTime> lastRun() const;

    void start();
    // The run that `due()` started ended. A failure counts toward the backoff unless `backoff` is false (no network).
    // `run` names the run it answers (currentRun() when it started); an answer for another run is ignored, which also
    // drops a late answer after the deadline. 0 means the current run.
    void finished(bool success, bool backoff = true, quint64 run = 0);
    // The run `due()` started last.
    quint64 currentRun() const { return m_run; }
    // Replaces the interval of automatic runs (SPEC §13: the 6 h unsupported-plan re-check, ADR-018) until nullopt; the
    // manual floor stays.
    void setIntervalOverride(std::optional<qint64> ms);
    // A run that has not finished after `ms` ends as a failure, so no lost answer stops the source for good.
    void setRunDeadline(int ms);
    // An extra trigger at `at` (a window's reset time, rule 8).
    void trigger(const QDateTime &at);
    // The popup opened: a trigger now if the minimum interval has passed since the last run, else dropped (ADR-019).
    bool triggerIfDue();
    // Runs as soon as the manual floor allows, ignoring interval and backoff (rule 4).
    void requestManual();
    // No run before `until`, not even a manual one (ADR-020); cleared by a success.
    void holdUntil(const QDateTime &until);
    // No automatic runs; a manual refresh still runs (ADR-022).
    void setPaused(bool paused);
    // A new policy (a changed interval setting) applies from the next run.
    void setPolicy(SchedulePolicy policy);
    // A new account: forgets runs, failures, triggers, hold and pause, ends the run in flight (its result is the old
    // account's and the source drops it) and runs now.
    void reset();

Q_SIGNALS:
    void due();

private:
    void arm();
    void fire();

    SchedulePolicy m_policy;
    QPointer<ScheduleConditionsSource> m_conditions;
    QTimer m_timer;
    QTimer m_deadline;
    std::optional<qint64> m_intervalOverride;
    quint64 m_run = 0;
    bool m_wasOffline = false;
    std::optional<QDateTime> m_lastSuccess;
    std::optional<QDateTime> m_lastAttempt;
    std::optional<QDateTime> m_hold;
    QList<QDateTime> m_triggers;
    int m_failures = 0;
    bool m_started = false;
    bool m_running = false;
    bool m_manual = false;
    bool m_paused = false;
};

} // namespace contribusage
