#include "core/historystore.h"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QTest>

using namespace contribusage;

namespace {

ActivityDay day(const char *iso, qint64 requests) {
    return {.day = QDate::fromString(QString::fromLatin1(iso), Qt::ISODate),
            .requests = requests,
            .sessions = 1,
            .tokens = {.input = requests, .output = 0, .cacheWrite = 0, .cacheRead = 0},
            .byModel = {}};
}

QList<QDate> dates(const QList<ActivityDay> &days) {
    QList<QDate> list;
    for (const ActivityDay &item : days) {
        list.append(item.day);
    }
    return list;
}

const QDateTime now(QDate(2026, 10, 5), QTime(12, 0));

} // namespace

// SPEC §8.3.4, ported from HistoryStoreTests.swift.
class TestHistoryStore : public QObject {
    Q_OBJECT

private Q_SLOTS:
    // now - 48 h is 10-03 12:00, so 10-01 and 10-02 freeze; 10-03 onwards stays live.
    void freezesDaysMoreThan48HoursPast() {
        QTemporaryDir dir;
        const AppPaths paths{.root = dir.path()};
        auto store = HistoryStore::open(QStringLiteral("claude-code"), paths);
        QVERIFY(store);
        const QList<ActivityDay> live{day("2026-10-01", 1), day("2026-10-02", 2), day("2026-10-03", 3),
                                      day("2026-10-04", 4), day("2026-10-05", 5)};
        QCOMPARE(store->merge(live, now), live);
        QCOMPARE(dates(store->frozen()), (QList<QDate>{QDate(2026, 10, 1), QDate(2026, 10, 2)}));
        const auto reopened = HistoryStore::open(QStringLiteral("claude-code"), paths);
        QVERIFY(reopened);
        QCOMPARE(reopened->frozen(), store->frozen());
        QVERIFY(QFile::exists(dir.filePath(QStringLiteral("providers/claude-code/history.json"))));
    }

    // Review Focus 2: a frozen day wins over its live count and stays when its transcripts are gone.
    void frozenDaysAreNeverRecomputed() {
        QTemporaryDir dir;
        const AppPaths paths{.root = dir.path()};
        auto store = HistoryStore::open(QStringLiteral("claude-code"), paths);
        QVERIFY(store);
        store->merge({day("2026-10-01", 5)}, now);
        QCOMPARE(store->merge({day("2026-10-01", 9)}, now), QList<ActivityDay>{day("2026-10-01", 5)});
        QCOMPARE(store->merge({day("2026-10-05", 1)}, now),
                 (QList<ActivityDay>{day("2026-10-01", 5), day("2026-10-05", 1)}));
    }

    // A frozen day is shown once, also when a clock change moves it back past the cut-off.
    void aFrozenDayShowsOnceWhenTheCutoffMovesBack() {
        QTemporaryDir dir;
        auto store = HistoryStore::open(QStringLiteral("claude-code"), {.root = dir.path()});
        QVERIFY(store);
        store->merge({day("2026-10-02", 2)}, now);
        const QList<ActivityDay> shown = store->merge({day("2026-10-02", 7)}, now.addDays(-3));
        QCOMPARE(shown, QList<ActivityDay>{day("2026-10-02", 2)});
    }

    // 365 days: 2025-10-06 is 364 days before 2026-10-05 and stays; the day before goes.
    void keeps365Days() {
        QTemporaryDir dir;
        auto store = HistoryStore::open(QStringLiteral("claude-code"), {.root = dir.path()});
        QVERIFY(store);
        store->merge({day("2025-10-05", 1), day("2025-10-06", 2)}, now);
        QCOMPARE(dates(store->frozen()), QList<QDate>{QDate(2025, 10, 6)});
    }

    void historiesAreKeptPerProvider() {
        QTemporaryDir dir;
        const AppPaths paths{.root = dir.path()};
        auto first = HistoryStore::open(QStringLiteral("claude-code"), paths);
        auto second = HistoryStore::open(QStringLiteral("other"), paths);
        QVERIFY(first && second);
        first->merge({day("2026-10-01", 1)}, now);
        QVERIFY(second->frozen().isEmpty());
        QVERIFY(HistoryStore::open(QStringLiteral("other"), paths)->frozen().isEmpty());
    }

    // Review Focus 4: an unreadable file is an error and stays as it is.
    void unreadableHistoryIsNeverDiscarded() {
        QTemporaryDir dir;
        const AppPaths paths{.root = dir.path()};
        QVERIFY(QDir().mkpath(dir.filePath(QStringLiteral("providers/claude-code"))));
        const QString file = dir.filePath(QStringLiteral("providers/claude-code/history.json"));
        {
            QFile out(file);
            QVERIFY(out.open(QIODevice::WriteOnly));
            out.write("not json");
        }
        QVERIFY(!HistoryStore::open(QStringLiteral("claude-code"), paths));
        QFile in(file);
        QVERIFY(in.open(QIODevice::ReadOnly));
        QCOMPARE(in.readAll(), QByteArray("not json"));
    }
};

QTEST_GUILESS_MAIN(TestHistoryStore)
#include "tst_historystore.moc"
