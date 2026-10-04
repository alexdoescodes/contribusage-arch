#include "core/jsonstore.h"
#include "core/limitscache.h"

#include <QFile>
#include <QTemporaryDir>
#include <QTest>
#include <QTimeZone>

using namespace contribusage;

namespace {

Snapshot<LimitsReport> snapshot(const QString &provider, double percent) {
    return {.value = LimitsReport{.provider = provider,
                                  .windows = {UsageWindow{.label = QStringLiteral("Current session"),
                                                          .kind = WindowKind::Session,
                                                          .usedPercent = percent,
                                                          .isBelowOne = false,
                                                          .resetsAt = std::nullopt}},
                                  .billingNote = std::nullopt,
                                  .insights = std::nullopt,
                                  .rawOutput = std::nullopt},
            .fetchedAt = QDateTime(QDate(2026, 10, 5), QTime(1, 0), QTimeZone::UTC),
            .origin = origins::poll()};
}

} // namespace

// FR-10, SPEC §10.7: state.json holds the last limits snapshot per provider; it is a cache.
class TestLimitsCache : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void roundTrips() {
        QTemporaryDir temporary;
        const AppPaths paths{.root = temporary.path()};
        QVERIFY(!limitscache::load(paths, QStringLiteral("claude-code")));
        QVERIFY(limitscache::save(paths, snapshot(QStringLiteral("claude-code"), 23)));
        const auto loaded = limitscache::load(paths, QStringLiteral("claude-code"));
        QVERIFY(loaded);
        QVERIFY(*loaded == snapshot(QStringLiteral("claude-code"), 23));
        QVERIFY(QFile::exists(temporary.path() + QStringLiteral("/state.json")));
    }

    void keepsOtherProviders() {
        QTemporaryDir temporary;
        const AppPaths paths{.root = temporary.path()};
        QVERIFY(limitscache::save(paths, snapshot(QStringLiteral("claude-code"), 23)));
        QVERIFY(limitscache::save(paths, snapshot(QStringLiteral("other"), 50)));
        QVERIFY(limitscache::save(paths, snapshot(QStringLiteral("claude-code"), 30)));
        QCOMPARE(limitscache::load(paths, QStringLiteral("other"))->value.windows.at(0).usedPercent, 50.0);
        QCOMPARE(limitscache::load(paths, QStringLiteral("claude-code"))->value.windows.at(0).usedPercent, 30.0);
    }

    // A cache of another version or a broken one reads as empty, and the next save replaces it.
    void anotherVersionReadsAsEmptyAndIsReplaced() {
        QTemporaryDir temporary;
        const AppPaths paths{.root = temporary.path()};
        QVERIFY(jsonstore::write(paths.root + QStringLiteral("/state.json"), 99, QJsonObject()));
        QVERIFY(!limitscache::load(paths, QStringLiteral("claude-code")));
        QVERIFY(limitscache::save(paths, snapshot(QStringLiteral("claude-code"), 23)));
        QVERIFY(limitscache::load(paths, QStringLiteral("claude-code")));
    }
};

QTEST_GUILESS_MAIN(TestLimitsCache)
#include "tst_limitscache.moc"
