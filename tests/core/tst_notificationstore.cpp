#include "core/limitscache.h"
#include "core/notificationstore.h"

#include <QTemporaryDir>
#include <QTest>

using namespace contribusage;

// FR-15: the planner's cycles survive a restart, in state.json beside the other caches.
class TestNotificationStore : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void roundTripsBesideTheLimits() {
        QTemporaryDir dir;
        const AppPaths paths{.root = dir.path()};
        const QDateTime sentAt(QDate(2026, 10, 5), QTime(9, 0), QTimeZone::UTC);
        NotificationsSent sent;
        sent[QStringLiteral("claude-code")][QStringLiteral("Current session")] =
            NotificationCycle{.resetsAt = sentAt.addSecs(3600), .threshold = 80, .sentAt = sentAt};
        sent[QStringLiteral("claude-code")][QStringLiteral("Current week (all models)")] =
            NotificationCycle{.resetsAt = std::nullopt, .threshold = 95, .sentAt = sentAt};
        const Snapshot<LimitsReport> limits{.value = {.provider = QStringLiteral("claude-code"),
                                                      .windows = {},
                                                      .billingNote = {},
                                                      .insights = {},
                                                      .rawOutput = {}},
                                            .fetchedAt = sentAt,
                                            .origin = origins::poll()};
        QVERIFY(limitscache::save(paths, limits));
        QVERIFY(notificationstore::save(paths, sent));
        QCOMPARE(notificationstore::load(paths), sent);
        QVERIFY(limitscache::load(paths, QStringLiteral("claude-code"))); // kept
    }

    void aMissingOrBrokenEntryIsEmpty() {
        QTemporaryDir dir;
        QVERIFY(notificationstore::load({.root = dir.path()}).isEmpty());
    }
};

QTEST_GUILESS_MAIN(TestNotificationStore)
#include "tst_notificationstore.moc"
