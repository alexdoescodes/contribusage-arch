#include "core/widgetsettings.h"

#include <QTest>

using namespace contribusage;

// SPEC §10.7, §11.6: the plasmoid configuration as validated settings.
class TestWidgetSettings : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void defaultsFollowSpec() {
        const WidgetSettings settings = widgetSettingsFromMap({});
        QCOMPARE(settings.menuBarMode, MenuBarMode::Primary);
        QCOMPARE(settings.probeIntervalMinutes, 5);
        QVERIFY(settings.githubEnabled);
        QCOMPARE(settings.githubIntervalMinutes, 30);
        QCOMPARE(settings.notifications.thresholds, (QList<int>{80, 95}));
        QVERIFY(!settings.notifications.notifyOnReset);
    }

    // Review Focus 2: out of range values are clamped or dropped.
    void settingsAreClampedAndFiltered() {
        const WidgetSettings settings =
            widgetSettingsFromMap({{QStringLiteral("probeIntervalMinutes"), 0},
                                   {QStringLiteral("githubIntervalMinutes"), 9999},
                                   {QStringLiteral("notificationThresholds"), QStringLiteral("120,70,45,90,60")},
                                   {QStringLiteral("menuBarMode"), QStringLiteral("nonsense")},
                                   {QStringLiteral("notifyOnReset"), true},
                                   {QStringLiteral("githubEnabled"), false},
                                   {QStringLiteral("claudePathOverride"), QStringLiteral(" /opt/claude ")}});
        QCOMPARE(settings.probeIntervalMinutes, 1);
        QCOMPARE(settings.githubIntervalMinutes, 360);
        QCOMPARE(settings.notifications.thresholds, (QList<int>{60, 70, 90}));
        QCOMPARE(settings.menuBarMode, MenuBarMode::Primary);
        QVERIFY(settings.notifications.notifyOnReset);
        QVERIFY(!settings.githubEnabled);
        QCOMPARE(settings.claudePathOverride, QStringLiteral("/opt/claude"));
    }

    void modeNames() {
        QCOMPARE(menuBarModeFromName(QStringLiteral("weekly")), MenuBarMode::Weekly);
        QCOMPARE(menuBarModeFromName(QStringLiteral("highest")), MenuBarMode::Highest);
        QCOMPARE(menuBarModeFromName(QStringLiteral("githubToday")), MenuBarMode::GithubToday);
        QCOMPARE(menuBarModeFromName(QStringLiteral("primaryAndGitHub")), MenuBarMode::PrimaryAndGitHub);
        QCOMPARE(menuBarModeFromName(QStringLiteral("iconOnly")), MenuBarMode::IconOnly);
    }
};

QTEST_GUILESS_MAIN(TestWidgetSettings)
#include "tst_widgetsettings.moc"
