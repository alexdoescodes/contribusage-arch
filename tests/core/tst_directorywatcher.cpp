#include "core/directorywatcher.h"

#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

using namespace contribusage;

namespace {

void append(const QString &path, const QByteArray &bytes) {
    QFile file(path);
    QVERIFY(file.open(QIODevice::Append));
    file.write(bytes);
}

} // namespace

// Port design §1 (FileEvents): inotify through QFileSystemWatcher, recursive, debounced.
class TestDirectoryWatcher : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void reportsNewFoldersFilesAndAppends() {
        QTemporaryDir dir;
        const QString root = dir.filePath(QStringLiteral("projects"));
        QVERIFY(QDir().mkpath(root));
        DirectoryWatcher watcher({root}, QStringLiteral(".jsonl"), 50);
        QSignalSpy spy(&watcher, &DirectoryWatcher::changed);
        watcher.start();
        QVERIFY(QDir().mkpath(root + QStringLiteral("/p/sub")));
        QTRY_COMPARE(spy.size(), 1);
        append(root + QStringLiteral("/p/sub/s.jsonl"), "a\n");
        QTRY_COMPARE(spy.size(), 2);
        append(root + QStringLiteral("/p/sub/s.jsonl"), "b\n");
        QTRY_COMPARE(spy.size(), 3);
    }

    void aBurstIsOneChange() {
        QTemporaryDir dir;
        const QString root = dir.filePath(QStringLiteral("projects"));
        QVERIFY(QDir().mkpath(root));
        append(root + QStringLiteral("/s.jsonl"), "a\n");
        DirectoryWatcher watcher({root}, QStringLiteral(".jsonl"), 200);
        QSignalSpy spy(&watcher, &DirectoryWatcher::changed);
        watcher.start();
        for (int index = 0; index < 5; ++index) {
            append(root + QStringLiteral("/s.jsonl"), "x\n");
        }
        QTRY_COMPARE(spy.size(), 1);
        QTest::qWait(400);
        QCOMPARE(spy.size(), 1);
    }

    // SPEC §13: a root that appears later is watched from then on.
    void aRootCreatedLaterIsWatched() {
        QTemporaryDir dir;
        const QString root = dir.filePath(QStringLiteral("projects"));
        DirectoryWatcher watcher({root}, QStringLiteral(".jsonl"), 50);
        QSignalSpy spy(&watcher, &DirectoryWatcher::changed);
        watcher.start();
        QVERIFY(QDir().mkpath(root));
        QTRY_VERIFY(spy.size() >= 1);
        const qsizetype before = spy.size();
        append(root + QStringLiteral("/s.jsonl"), "a\n");
        QTRY_VERIFY(spy.size() > before);
    }

    void otherFilesAreNotWatched() {
        QTemporaryDir dir;
        const QString root = dir.filePath(QStringLiteral("projects"));
        QVERIFY(QDir().mkpath(root));
        append(root + QStringLiteral("/notes.txt"), "a\n");
        DirectoryWatcher watcher({root}, QStringLiteral(".jsonl"), 50);
        QSignalSpy spy(&watcher, &DirectoryWatcher::changed);
        watcher.start();
        append(root + QStringLiteral("/notes.txt"), "b\n");
        QTest::qWait(300);
        QCOMPARE(spy.size(), 0);
    }

    // A change in one folder looks at that folder only, not the whole tree (the GUI thread pays for it).
    void aChangeDoesNotWalkTheWholeTree() {
        QTemporaryDir dir;
        const QString root = dir.filePath(QStringLiteral("projects"));
        for (int folder = 0; folder < 300; ++folder) {
            const QString path = root + QStringLiteral("/p") + QString::number(folder);
            QVERIFY(QDir().mkpath(path));
            for (int file = 0; file < 40; ++file) {
                QFile out(path + QStringLiteral("/f") + QString::number(file) + QStringLiteral(".txt"));
                QVERIFY(out.open(QIODevice::WriteOnly));
            }
        }
        DirectoryWatcher watcher({root}, QStringLiteral(".jsonl"), 0);
        QSignalSpy spy(&watcher, &DirectoryWatcher::changed);
        watcher.start();
        QElapsedTimer timer;
        timer.start();
        for (int index = 0; index < 30; ++index) {
            append(root + QStringLiteral("/p") + QString::number(index) + QStringLiteral("/s.jsonl"), "a\n");
            QElapsedTimer limit; // QTRY_* sleeps 50 ms per poll, which would swamp what is measured here
            limit.start();
            while (spy.size() <= index && limit.elapsed() < 5'000) {
                QCoreApplication::processEvents(QEventLoop::AllEvents, 5);
            }
            QVERIFY(spy.size() > index);
        }
        QVERIFY2(timer.elapsed() < 100, qPrintable(QString::number(timer.elapsed()) + QStringLiteral(" ms")));
    }
};

QTEST_GUILESS_MAIN(TestDirectoryWatcher)
#include "tst_directorywatcher.moc"
