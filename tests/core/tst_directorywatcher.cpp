#include "core/directorywatcher.h"

#include <QDir>
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
};

QTEST_GUILESS_MAIN(TestDirectoryWatcher)
#include "tst_directorywatcher.moc"
