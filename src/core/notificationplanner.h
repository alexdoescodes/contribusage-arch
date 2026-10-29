#pragma once

#include "core/reports.h"

#include <QDateTime>
#include <QList>
#include <QMap>
#include <QString>

#include <functional>
#include <optional>

namespace contribusage {

// FR-15: what was sent in one window's reset cycle; the highest threshold stands for every lower one.
struct NotificationCycle {
    std::optional<QDateTime> resetsAt;
    int threshold = 0;
    QDateTime sentAt; // FR-15 prunes the cycle 8 days later

    bool operator==(const NotificationCycle &) const = default;
};

// The cycles by provider and window label, as state.json will store them (L-5).
using NotificationsSent = QMap<QString, QMap<QString, NotificationCycle>>;

struct NotificationNote {
    QString id;       // one per window and cycle: provider|label|reset seconds or "-" (FR-52)
    QString provider; // the notification's group
    QString title;    // starts with the provider's display name

    bool operator==(const NotificationNote &) const = default;
};

// `notificationThresholds` and `notifyOnReset` (SPEC §10.7).
struct NotificationSettings {
    QList<int> thresholds{80, 95};
    bool notifyOnReset = false;
};

// FR-13 to FR-15, ADR-027, ADR-038: the notifications a fetched report brings. Updates `sent` and drops cycles sent
// more than 8 days ago. Pure: the applet delivers the notes (L-5).
QList<NotificationNote> planNotifications(const LimitsReport &report, const QString &displayName,
                                          const std::function<QString(const QString &)> &windowTitle,
                                          const NotificationSettings &settings, NotificationsSent &sent,
                                          const QDateTime &now);

} // namespace contribusage
