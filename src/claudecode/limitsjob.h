#pragma once

#include "claudecode/claudecodelimits.h"
#include "core/snapshot.h"

#include <QObject>
#include <QTimer>

namespace contribusage::claudecode {

// The limits source on an interval (ADR-039), with FR-10's cache and SPEC §11.3's states. A stand-in for the refresh
// coordinator of L-1c, which takes over the schedule and its triggers.
class LimitsJob : public QObject {
    Q_OBJECT

public:
    LimitsJob(ClaudeCodeLimits &limits, AppPaths paths, int intervalMs, QObject *parent = nullptr);

    const SourceState<LimitsReport> &state() const { return m_state; }

    // Shows the cached snapshot, probes now, then every interval.
    void start();
    // Probes now unless a probe is running.
    void refresh();

Q_SIGNALS:
    void stateChanged();

private:
    void finish(LimitsOutcome outcome);

    ClaudeCodeLimits &m_limits;
    AppPaths m_paths;
    QTimer m_timer;
    bool m_fetching = false;
    SourceState<LimitsReport> m_state = SourceState<LimitsReport>::loading(std::nullopt);
};

} // namespace contribusage::claudecode
