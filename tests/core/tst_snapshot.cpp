#include "core/snapshot.h"

#include <QTest>
#include <QTimeZone>

using namespace contribusage;

namespace {
const Snapshot<int> fetched{
    .value = 1, .fetchedAt = QDateTime::fromSecsSinceEpoch(0, QTimeZone::UTC), .origin = origins::cache()};
}

class TestSnapshot : public QObject {
    Q_OBJECT

private Q_SLOTS:
    // SPEC §12 "stale after".
    void snapshotIsStaleOnlyPastTheLimit() {
        QVERIFY(!fetched.isStale(QDateTime::fromSecsSinceEpoch(1800, QTimeZone::UTC), 1800));
        QVERIFY(fetched.isStale(QDateTime::fromSecsSinceEpoch(1801, QTimeZone::UTC), 1800));
    }

    // SPEC §11.3: every state but "not configured" and "first load" can show values.
    void sourceStateExposesTheSnapshotItCanShow() {
        QVERIFY(!SourceState<int>::notConfigured(NotConfiguredReason::NoLocalData).snapshot());
        QVERIFY(!SourceState<int>::loading(std::nullopt).snapshot());
        QVERIFY(SourceState<int>::loading(fetched).snapshot() == fetched);
        QVERIFY(SourceState<int>::loaded(fetched).snapshot() == fetched);
        const SourceError offline{.kind = SourceError::Kind::Offline};
        QVERIFY(SourceState<int>::failed(offline, fetched).snapshot() == fetched);
        QVERIFY(!SourceState<int>::failed(offline, std::nullopt).snapshot());
    }

    void sourceStateKeepsItsDetails() {
        const auto unsupported =
            SourceState<int>::notConfigured(NotConfiguredReason::UnsupportedPlan, QStringLiteral("Needs a login"));
        QCOMPARE(unsupported.phase, SourceState<int>::Phase::NotConfigured);
        QCOMPARE(unsupported.reason, NotConfiguredReason::UnsupportedPlan);
        QCOMPARE(unsupported.note, QStringLiteral("Needs a login"));
        const auto failed = SourceState<int>::failed({.kind = SourceError::Kind::TimedOut}, std::nullopt);
        QCOMPARE(failed.phase, SourceState<int>::Phase::Failed);
        QVERIFY(failed.error && failed.error->kind == SourceError::Kind::TimedOut);
    }
};

QTEST_GUILESS_MAIN(TestSnapshot)
#include "tst_snapshot.moc"
