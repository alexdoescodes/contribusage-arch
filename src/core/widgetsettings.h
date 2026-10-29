#pragma once

#include "core/menubarlabel.h"
#include "core/notificationplanner.h"

#include <QString>
#include <QVariantMap>

namespace contribusage {

// SPEC §10.7's settings as the widget stores them in its plasmoid configuration (config/main.xml), validated.
struct WidgetSettings {
    MenuBarMode menuBarMode = MenuBarMode::Primary;
    int probeIntervalMinutes = 5;   // 1 to 60 (SPEC §11.6)
    bool githubEnabled = true;      // FR-43
    int githubIntervalMinutes = 30; // 10 to 360
    NotificationSettings notifications;
    QString claudePathOverride;
};

// `menuBarMode` by its SPEC §10.7 name; unknown names give Primary.
MenuBarMode menuBarModeFromName(const QString &name);

// From Plasmoid.configuration's values; missing keys keep the defaults, out of range values are clamped, thresholds
// outside 50 to 99 are dropped and at most 3 kept (FR-13).
WidgetSettings widgetSettingsFromMap(const QVariantMap &map);

} // namespace contribusage
