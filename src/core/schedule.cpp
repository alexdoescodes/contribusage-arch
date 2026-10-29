#include "core/schedule.h"

#include <algorithm>

namespace contribusage {

namespace {

// Clamped to the bounds; 2×, 4×, 8× after failures up to the maximum; doubled in low power mode.
qint64 interval(const SchedulePolicy &policy, int failures, bool lowPower) {
    qint64 value = std::min(std::max(policy.defaultMs, policy.minimumMs), policy.maximumMs);
    value = std::min(value * (qint64(1) << std::min(failures, 3)), policy.maximumMs);
    return lowPower ? value * 2 : value;
}

} // namespace

std::optional<QDateTime> nextRun(const SchedulePolicy &policy, const std::optional<QDateTime> &lastSuccess,
                                 const std::optional<QDateTime> &lastAttempt, int failures, const QDateTime &now,
                                 const ScheduleConditions &conditions, bool manual, const QList<QDateTime> &triggers) {
    if (conditions.asleep || (policy.needsNetwork && !conditions.online)) {
        return std::nullopt;
    }
    std::optional<QDateTime> wake;
    if (conditions.lastWake) {
        wake = conditions.lastWake->addMSecs(wakeDelayMs);
    }
    std::optional<QDateTime> last = lastSuccess;
    if (lastAttempt && (!last || *lastAttempt > *last)) {
        last = lastAttempt;
    }
    QDateTime next = now;
    if (last) {
        next = last->addMSecs(manual ? policy.manualFloorMs : interval(policy, failures, conditions.lowPower));
        QList<QDateTime> candidates = triggers;
        if (wake) {
            candidates.append(*wake);
        }
        std::optional<QDateTime> first;
        for (const QDateTime &trigger : std::as_const(candidates)) {
            if (trigger > *last && (!first || trigger < *first)) {
                first = trigger;
            }
        }
        if (first) {
            next = std::min(next, std::max(*first, last->addMSecs(policy.minimumMs)));
        }
    }
    if (wake) {
        next = std::max(next, *wake);
    }
    return next;
}

} // namespace contribusage
