#include "core/notificationplanner.h"

#include <algorithm>
#include <cmath>

namespace contribusage {

namespace {

NotificationNote note(const QString &provider, const QString &label, const std::optional<QDateTime> &resetsAt,
                      const QString &title) {
    const QString reset = resetsAt ? QString::number(resetsAt->toSecsSinceEpoch()) : QStringLiteral("-");
    return {.id = QString(provider + u'|' + label + u'|' + reset), .provider = provider, .title = title};
}

} // namespace

QList<NotificationNote> planNotifications(const LimitsReport &report, const QString &displayName,
                                          const std::function<QString(const QString &)> &windowTitle,
                                          const NotificationSettings &settings, NotificationsSent &sent,
                                          const QDateTime &now) {
    // FR-13: up to 3 values between 50 and 99.
    QList<int> thresholds;
    for (const int value : settings.thresholds) {
        if (value >= 50 && value <= 99 && !thresholds.contains(value)) {
            thresholds.append(value);
        }
    }
    std::ranges::sort(thresholds);
    thresholds = thresholds.mid(0, 3);
    for (auto provider = sent.begin(); provider != sent.end();) {
        provider->removeIf([&now](const auto &entry) { return entry.value().sentAt.secsTo(now) > 8 * 86400; });
        provider = provider->isEmpty() ? sent.erase(provider) : std::next(provider);
    }
    QMap<QString, NotificationCycle> cycles = sent.value(report.provider);
    QList<NotificationNote> notes;
    for (const UsageWindow &window : report.windows) {
        if (window.resetsAt && *window.resetsAt <= now) {
            continue; // FR-11: unknown until the next refresh
        }
        const QString title = windowTitle(window.label);
        std::optional<NotificationCycle> cycle;
        if (cycles.contains(window.label)) {
            cycle = cycles.value(window.label);
        }
        // FR-15, ADR-038: only a move of more than 1 h ends the cycle; a window without a reset time ends it once the
        // cycle's own time has passed; a cycle without one is kept.
        if (cycle && cycle->resetsAt) {
            const QDateTime was = *cycle->resetsAt;
            const bool ended = window.resetsAt ? std::abs(was.secsTo(*window.resetsAt)) > 3600 : now > was;
            if (ended) {
                if (settings.notifyOnReset) {
                    notes.append(
                        note(report.provider, window.label, was,
                             QString(displayName + QStringLiteral(": ") + title + QStringLiteral(" has reset"))));
                }
                cycle.reset();
            }
        }
        // The cycle keeps its first known reset time, so its notes share one identifier (FR-52).
        const std::optional<QDateTime> resetsAt = cycle && cycle->resetsAt ? cycle->resetsAt : window.resetsAt;
        const int floor = cycle ? cycle->threshold : 0;
        std::optional<int> highest;
        for (const int threshold : std::as_const(thresholds)) {
            if (window.usedPercent >= threshold && threshold > floor) {
                highest = threshold;
            }
        }
        if (highest) {
            cycle = NotificationCycle{.resetsAt = resetsAt, .threshold = *highest, .sentAt = now};
            notes.append(note(report.provider, window.label, resetsAt,
                              QString(displayName + QStringLiteral(": ") + title + QStringLiteral(" at ") +
                                      QString::number(*highest) + QStringLiteral(" %"))));
        } else if (cycle) {
            cycle->resetsAt = resetsAt;
        }
        if (cycle) {
            cycles.insert(window.label, *cycle);
        } else {
            cycles.remove(window.label);
        }
    }
    if (cycles.isEmpty()) {
        sent.remove(report.provider);
    } else {
        sent.insert(report.provider, cycles);
    }
    return notes;
}

} // namespace contribusage
