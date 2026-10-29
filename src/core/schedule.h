#pragma once

#include <QDateTime>
#include <QList>

#include <optional>

namespace contribusage {

// SPEC §10.2: how often a polled source runs. Milliseconds, so tests can run real schedules in a few ms.
struct SchedulePolicy {
    qint64 defaultMs = 0;
    qint64 minimumMs = 0;
    qint64 maximumMs = 0;
    qint64 staleAfterMs = 0;
    qint64 manualFloorMs = 0;
    bool needsNetwork = false; // skipped while offline (SPEC §12 rule 2)

    // A fixed interval without a floor or network: for tests and local sources.
    static SchedulePolicy every(qint64 ms) {
        return {.defaultMs = ms,
                .minimumMs = ms,
                .maximumMs = ms,
                .staleAfterMs = 6 * ms,
                .manualFloorMs = 0,
                .needsNetwork = false};
    }
    bool operator==(const SchedulePolicy &) const = default;
};

// What the machine is doing (SPEC §12 rules 1 to 3), fed by the platform layer.
struct ScheduleConditions {
    bool online = true;
    bool lowPower = false;
    bool asleep = false;
    std::optional<QDateTime> lastWake;

    bool operator==(const ScheduleConditions &) const = default;
};

inline constexpr qint64 wakeDelayMs = 10'000;

// SPEC §12 rule 7: when a polled source runs next, possibly in the past (it is due); nullopt while asleep or while
// offline for a source that needs the network. `manual` replaces interval and backoff by the manual floor. `triggers`
// (popup opened, a window's reset), joined by the wake plus 10 s: the first one after the last run runs the source, but
// not within the minimum interval (rule 8).
std::optional<QDateTime> nextRun(const SchedulePolicy &policy, const std::optional<QDateTime> &lastSuccess,
                                 const std::optional<QDateTime> &lastAttempt, int failures, const QDateTime &now,
                                 const ScheduleConditions &conditions, bool manual = false,
                                 const QList<QDateTime> &triggers = {});

} // namespace contribusage
