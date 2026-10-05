#include "core/notificationplanner.h"

#include <QTest>

using namespace contribusage;

namespace {

const QDateTime now(QDate(2026, 5, 9), QTime(12, 0), QTimeZone::UTC);
const QDateTime cycle = now.addSecs(3600);

LimitsReport report(double percent, const QString &provider = QStringLiteral("a"),
                    std::optional<QDateTime> resetsAt = cycle) {
    return {.provider = provider,
            .windows = {{.label = QStringLiteral("Current session"),
                         .kind = WindowKind::Session,
                         .usedPercent = percent,
                         .isBelowOne = false,
                         .resetsAt = resetsAt}},
            .billingNote = std::nullopt,
            .insights = std::nullopt,
            .rawOutput = std::nullopt};
}

QList<NotificationNote> notes(const LimitsReport &input, NotificationsSent &sent,
                              const NotificationSettings &settings = {}, const QDateTime &at = now) {
    return planNotifications(
        input, QStringLiteral("Claude Code"), [](const QString &label) { return label; }, settings, sent, at);
}

QStringList titles(const LimitsReport &input, NotificationsSent &sent, const NotificationSettings &settings = {},
                   const QDateTime &at = now) {
    QStringList list;
    for (const NotificationNote &note : notes(input, sent, settings, at)) {
        list.append(note.title);
    }
    return list;
}

const NotificationSettings onReset{.thresholds = {80, 95}, .notifyOnReset = true};

} // namespace

// FR-13 to FR-15, US-3, ported from NotificationPlannerTests.swift.
class TestNotificationPlanner : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void eachThresholdFiresOncePerCycle() {
        NotificationsSent sent;
        QCOMPARE(titles(report(79), sent), QStringList());
        QCOMPARE(titles(report(80), sent), QStringList{QStringLiteral("Claude Code: Current session at 80 %")});
        QCOMPARE(titles(report(85), sent), QStringList());
        QCOMPARE(titles(report(96), sent), QStringList{QStringLiteral("Claude Code: Current session at 95 %")});
        QCOMPARE(titles(report(99), sent), QStringList());
        QCOMPARE(titles(report(99), sent, {.thresholds = {90, 95}, .notifyOnReset = false}), QStringList());
    }

    void aJumpOverTwoThresholdsSendsOnlyTheHighest() {
        NotificationsSent sent;
        QCOMPARE(titles(report(97), sent), QStringList{QStringLiteral("Claude Code: Current session at 95 %")});
        QCOMPARE(titles(report(99), sent), QStringList());
    }

    void aResetTimeMovingByUpToAnHourKeepsTheCycle() {
        NotificationsSent sent;
        const auto first = notes(report(85), sent, onReset);
        QCOMPARE(titles(report(85, QStringLiteral("a"), cycle.addSecs(-60)), sent, onReset), QStringList());
        QCOMPARE(titles(report(85, QStringLiteral("a"), cycle.addSecs(3600)), sent, onReset), QStringList());
        QCOMPARE(titles(report(85, QStringLiteral("a"), std::nullopt), sent, onReset), QStringList());
        const auto higher = notes(report(96, QStringLiteral("a"), cycle.addSecs(60)), sent, onReset);
        QCOMPARE(higher.size(), qsizetype(1));
        QCOMPARE(higher.at(0).title, QStringLiteral("Claude Code: Current session at 95 %"));
        QCOMPARE(higher.at(0).id, first.at(0).id);
        QCOMPARE(titles(report(10, QStringLiteral("a"), cycle.addSecs(3601)), sent, onReset),
                 QStringList{QStringLiteral("Claude Code: Current session has reset")});
    }

    void aCycleWithoutAResetTimeTakesTheFirstOneReported() {
        NotificationsSent sent;
        titles(report(85, QStringLiteral("a"), std::nullopt), sent);
        QCOMPARE(titles(report(85), sent), QStringList());
        QCOMPARE(titles(report(81, QStringLiteral("a"), cycle.addSecs(5 * 3600)), sent),
                 QStringList{QStringLiteral("Claude Code: Current session at 80 %")});
    }

    void aMissingResetTimeAfterTheCyclesResetEndsIt() {
        NotificationsSent sent;
        titles(report(85), sent, onReset);
        const QDateTime after = cycle.addSecs(60);
        QCOMPARE(titles(report(0.5, QStringLiteral("a"), std::nullopt), sent, onReset, after),
                 QStringList{QStringLiteral("Claude Code: Current session has reset")});
        QCOMPARE(titles(report(0.5, QStringLiteral("a"), std::nullopt), sent, onReset, after), QStringList());
        QCOMPARE(titles(report(81, QStringLiteral("a"), after.addSecs(5 * 3600)), sent, onReset, after),
                 QStringList{QStringLiteral("Claude Code: Current session at 80 %")});
    }

    void aResetReArmsAndNotifiesOnlyWhenAsked() {
        NotificationsSent sent;
        titles(report(90), sent);
        const QDateTime next = cycle.addSecs(5 * 3600);
        QCOMPARE(titles(report(10, QStringLiteral("a"), next), sent, {}, cycle), QStringList());
        QCOMPARE(titles(report(81, QStringLiteral("a"), next), sent, {}, cycle),
                 QStringList{QStringLiteral("Claude Code: Current session at 80 %")});
        NotificationsSent asked;
        titles(report(90), asked, onReset);
        QCOMPARE(titles(report(10, QStringLiteral("a"), next), asked, onReset, cycle),
                 QStringList{QStringLiteral("Claude Code: Current session has reset")});
        QCOMPARE(titles(report(10, QStringLiteral("a"), next), asked, onReset, cycle), QStringList());
    }

    void identicalLabelsFromTwoProvidersAreSeparate() {
        NotificationsSent sent;
        QCOMPARE(titles(report(85, QStringLiteral("a")), sent).size(), qsizetype(1));
        const auto other = notes(report(85, QStringLiteral("b")), sent);
        QCOMPARE(other.size(), qsizetype(1));
        QCOMPARE(other.at(0).provider, QStringLiteral("b"));
    }

    void thresholdsOutsideTheRangeAreIgnored() {
        NotificationsSent sent;
        const NotificationSettings settings{.thresholds = {10, 100, 60, 60, 70, 75, 90}, .notifyOnReset = false};
        QCOMPARE(titles(report(55), sent, settings), QStringList());
        QCOMPARE(titles(report(100), sent, settings),
                 QStringList{QStringLiteral("Claude Code: Current session at 75 %")});
    }

    void oldKeysArePrunedAndResetWindowsWait() {
        NotificationsSent sent;
        titles(report(85, QStringLiteral("a"), std::nullopt), sent);
        QCOMPARE(titles(report(85, QStringLiteral("a"), std::nullopt), sent, {}, now.addSecs(8 * 86400)),
                 QStringList());
        QCOMPARE(titles(report(85, QStringLiteral("a"), std::nullopt), sent, {}, now.addSecs(8 * 86400 + 1)).size(),
                 qsizetype(1));
        NotificationsSent passed;
        QCOMPARE(titles(report(85, QStringLiteral("a"), now), passed), QStringList());
    }

    void titlesUseTheWindowTitleAndKeysTheLabel() {
        NotificationsSent sent;
        const auto planned = planNotifications(
            report(80), QStringLiteral("Claude Code"),
            [](const QString &label) {
                return label == QStringLiteral("Current session") ? QStringLiteral("Aktuelle Sitzung") : label;
            },
            {}, sent, now);
        QCOMPARE(planned.at(0).title, QStringLiteral("Claude Code: Aktuelle Sitzung at 80 %"));
        QCOMPARE(sent.value(QStringLiteral("a")).firstKey(), QStringLiteral("Current session"));
    }
};

QTEST_GUILESS_MAIN(TestNotificationPlanner)
#include "tst_notificationplanner.moc"
