#pragma once

#include "claudecode/claudecodelimits.h"
#include "core/snapshot.h"

#include "core/notificationplanner.h"
#include "core/seams.h"
#include "core/sourceschedule.h"
#include <QObject>

namespace contribusage::claudecode {

// The limits source (ADR-039) with FR-10's cache and SPEC §11.3's states, run by its SourceSchedule (SPEC §12).
class LimitsJob : public QObject {
    Q_OBJECT

public:
    LimitsJob(ClaudeCodeLimits &limits, AppPaths paths, SchedulePolicy policy,
              ScheduleConditionsSource *conditions = nullptr, QObject *parent = nullptr);

    // SPEC §12, Claude Code row: 5 min, 1 min to 60 min, stale after 30 min, manual floor 30 s, needs the network.
    static SchedulePolicy defaultPolicy();

    const SourceState<LimitsReport> &state() const { return m_state; }

    // Shows the cached snapshot and runs the schedule.
    void start();
    // The manual refresh (SPEC §12 rule 4).
    void refresh();
    // SPEC §12: the popup opened; runs when the data is older than 5 min and the minimum interval has passed.
    void popupOpened();
    // A window's reset time (rule 8); the job adds its windows' times after each success.
    void addResetTrigger(const QDateTime &at);
    // FR-13 to FR-15: after each success the planner's notes go to `sink` (nullptr: none); its cycles persist in
    // state.json so a restart does not repeat them.
    void setNotifications(NotificationSink *sink, NotificationSettings settings);

Q_SIGNALS:
    void stateChanged();

private:
    void run();
    void finish(LimitsOutcome outcome, quint64 run);

    ClaudeCodeLimits &m_limits;
    AppPaths m_paths;
    SourceSchedule m_schedule;
    NotificationSink *m_sink = nullptr;
    NotificationSettings m_notificationSettings;
    NotificationsSent m_sent;
    SourceState<LimitsReport> m_state = SourceState<LimitsReport>::loading(std::nullopt);
};

} // namespace contribusage::claudecode
