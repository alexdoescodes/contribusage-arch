#include "core/widgetsettings.h"

#include <QStringList>

#include <algorithm>

namespace contribusage {

MenuBarMode menuBarModeFromName(const QString &name) {
    if (name == QStringLiteral("weekly")) {
        return MenuBarMode::Weekly;
    }
    if (name == QStringLiteral("highest")) {
        return MenuBarMode::Highest;
    }
    if (name == QStringLiteral("githubToday")) {
        return MenuBarMode::GithubToday;
    }
    if (name == QStringLiteral("primaryAndGitHub")) {
        return MenuBarMode::PrimaryAndGitHub;
    }
    if (name == QStringLiteral("iconOnly")) {
        return MenuBarMode::IconOnly;
    }
    return MenuBarMode::Primary;
}

WidgetSettings widgetSettingsFromMap(const QVariantMap &map) {
    WidgetSettings settings;
    settings.menuBarMode = menuBarModeFromName(map.value(QStringLiteral("menuBarMode")).toString());
    if (map.contains(QStringLiteral("probeIntervalMinutes"))) {
        settings.probeIntervalMinutes = std::clamp(map.value(QStringLiteral("probeIntervalMinutes")).toInt(), 1, 60);
    }
    if (map.contains(QStringLiteral("githubIntervalMinutes"))) {
        settings.githubIntervalMinutes =
            std::clamp(map.value(QStringLiteral("githubIntervalMinutes")).toInt(), 10, 360);
    }
    if (map.contains(QStringLiteral("githubEnabled"))) {
        settings.githubEnabled = map.value(QStringLiteral("githubEnabled")).toBool();
    }
    if (map.contains(QStringLiteral("notifyOnReset"))) {
        settings.notifications.notifyOnReset = map.value(QStringLiteral("notifyOnReset")).toBool();
    }
    if (map.contains(QStringLiteral("notificationThresholds"))) {
        QList<int> thresholds;
        for (const QString &part :
             map.value(QStringLiteral("notificationThresholds")).toString().split(u',', Qt::SkipEmptyParts)) {
            bool ok = false;
            const int value = part.trimmed().toInt(&ok);
            if (ok && value >= 50 && value <= 99 && !thresholds.contains(value)) {
                thresholds.append(value);
            }
        }
        std::ranges::sort(thresholds);
        settings.notifications.thresholds = thresholds.mid(0, 3);
    }
    settings.claudePathOverride = map.value(QStringLiteral("claudePathOverride")).toString().trimmed();
    return settings;
}

} // namespace contribusage
