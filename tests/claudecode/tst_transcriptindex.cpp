#include "claudecode/transcriptindex.h"

#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QTest>

using namespace contribusage;
using namespace contribusage::claudecode;

namespace {

QByteArray entry(const char *timestamp, const char *message, const char *request, const char *session,
                 const char *model = "claude-opus-5-5", int input = 10) {
    QJsonObject inner{{QStringLiteral("model"), QString::fromLatin1(model)},
                      {QStringLiteral("usage"),
                       QJsonObject{{QStringLiteral("input_tokens"), input}, {QStringLiteral("output_tokens"), 1}}}};
    if (message) {
        inner.insert(QStringLiteral("id"), QString::fromLatin1(message));
    }
    QJsonObject line{{QStringLiteral("type"), QStringLiteral("assistant")},
                     {QStringLiteral("timestamp"), QString::fromLatin1(timestamp)},
                     {QStringLiteral("sessionId"), QString::fromLatin1(session)},
                     {QStringLiteral("message"), inner}};
    if (request) {
        line.insert(QStringLiteral("requestId"), QString::fromLatin1(request));
    }
    return QJsonDocument(line).toJson(QJsonDocument::Compact) + '\n';
}

void write(const QString &path, const QByteArray &bytes, QIODevice::OpenMode mode = QIODevice::WriteOnly) {
    QVERIFY(QDir().mkpath(QFileInfo(path).absolutePath()));
    QFile file(path);
    QVERIFY(file.open(mode));
    file.write(bytes);
}

const QTimeZone utc = QTimeZone::UTC;

} // namespace

// SPEC §8.3.1, §8.3.3, FR-22, FR-24, FR-28; ported from TranscriptAggregatorTests, TranscriptFilesTests and
// TranscriptActivityTests.
class TestTranscriptIndex : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void rootsInOrder() {
        QCOMPARE(transcriptRoots(QStringLiteral("/home/u"), QStringLiteral("/cfg/claude")),
                 (QStringList{QStringLiteral("/cfg/claude/projects"), QStringLiteral("/home/u/.claude/projects"),
                              QStringLiteral("/home/u/.config/claude/projects")}));
        QCOMPARE(transcriptRoots(QStringLiteral("/home/u"), QStringLiteral("/home/u/.claude/")),
                 (QStringList{QStringLiteral("/home/u/.claude/projects"),
                              QStringLiteral("/home/u/.config/claude/projects")}));
        QCOMPARE(transcriptRoots(QStringLiteral("/home/u"), QString()).size(), qsizetype(2));
        QCOMPARE(transcriptRoots(QStringLiteral("/home/u"), QStringLiteral("relative")).size(), qsizetype(2));
    }

    // FR-28: the probe's project folder never counts; nested folders do.
    void filesSkipTheProbeProject() {
        QTemporaryDir dir;
        const QString root = dir.filePath(QStringLiteral("projects"));
        const QString probe = QStringLiteral("/home/u/.local/share/contribusage/providers/claude-code/probe");
        write(root + QStringLiteral("/-home-u-code/a.jsonl"), "x\n");
        write(root + QStringLiteral("/-home-u-code/sub/agent.jsonl"), "x\n");
        write(root + QStringLiteral("/-home-u-code/notes.txt"), "x\n");
        write(root + QStringLiteral("/-home-u--local-share-contribusage-providers-claude-code-probe/p.jsonl"), "x\n");
        QStringList files = transcriptFiles({root, dir.filePath(QStringLiteral("missing"))}, probe);
        files.sort();
        QCOMPARE(files, (QStringList{root + QStringLiteral("/-home-u-code/a.jsonl"),
                                     root + QStringLiteral("/-home-u-code/sub/agent.jsonl")}));
    }

    void duplicateKeyCountsOnce() {
        QTemporaryDir dir;
        const QString file = dir.filePath(QStringLiteral("a.jsonl"));
        write(file, entry("2026-10-01T10:00:00Z", "m1", "r1", "s1") + entry("2026-10-01T10:00:00Z", "m1", "r1", "s1"));
        TranscriptIndex index;
        index.update({file});
        const auto days = index.days(utc);
        QCOMPARE(days.size(), qsizetype(1));
        QCOMPARE(days.at(0).requests, qint64(1));
        QCOMPARE(days.at(0).tokens.input, qint64(10));
    }

    // FR-24: message id alone is a key; a line without both counts every time.
    void keyFallsBackToMessageIdThenCountsEachLine() {
        QTemporaryDir dir;
        const QString file = dir.filePath(QStringLiteral("a.jsonl"));
        write(file, entry("2026-10-01T10:00:00Z", "m1", nullptr, "s1") +
                        entry("2026-10-01T10:00:00Z", "m1", nullptr, "s1") +
                        entry("2026-10-01T10:00:00Z", nullptr, nullptr, "s1") +
                        entry("2026-10-01T10:00:00Z", nullptr, nullptr, "s1"));
        TranscriptIndex index;
        index.update({file});
        QCOMPARE(index.days(utc).at(0).requests, qint64(3));
    }

    void sessionsAreUniquePerDayAndModelsSplit() {
        QTemporaryDir dir;
        const QString file = dir.filePath(QStringLiteral("a.jsonl"));
        write(file, entry("2026-10-01T10:00:00Z", "m1", "r1", "s1", "opus", 10) +
                        entry("2026-10-01T11:00:00Z", "m2", "r2", "s1", "sonnet", 20) +
                        entry("2026-10-01T12:00:00Z", "m3", "r3", "s2", "opus", 30) +
                        entry("2026-10-02T12:00:00Z", "m4", "r4", "s1", "opus", 40));
        TranscriptIndex index;
        index.update({file});
        const auto days = index.days(utc);
        QCOMPARE(days.size(), qsizetype(2));
        QCOMPARE(days.at(0).sessions, qint64(2));
        QCOMPARE(days.at(0).byModel.value(QStringLiteral("opus")).input, qint64(40));
        QCOMPARE(days.at(0).byModel.value(QStringLiteral("sonnet")).input, qint64(20));
        QCOMPARE(days.at(1).sessions, qint64(1));
    }

    // SPEC §8.3.3: the day is the local day of the given zone.
    void dayUsesTheZone() {
        QTemporaryDir dir;
        const QString file = dir.filePath(QStringLiteral("a.jsonl"));
        write(file, entry("2026-10-01T23:30:00Z", "m1", "r1", "s1"));
        TranscriptIndex index;
        index.update({file});
        QCOMPARE(index.days(QTimeZone("Europe/Berlin")).at(0).day, QDate(2026, 10, 2));
        QCOMPARE(index.days(utc).at(0).day, QDate(2026, 10, 1));
    }

    // Review Focus 3: a resumed session repeats responses in a second file; the first file in path order counts.
    void firstDuplicateInPathOrderCounts() {
        QTemporaryDir dir;
        const QString first = dir.filePath(QStringLiteral("a.jsonl"));
        const QString second = dir.filePath(QStringLiteral("b.jsonl"));
        write(first, entry("2026-10-01T10:00:00Z", "m1", "r1", "s1", "opus", 10));
        write(second, entry("2026-10-01T10:00:00Z", "m1", "r1", "s2", "opus", 99));
        TranscriptIndex index;
        index.update({second, first});
        const auto days = index.days(utc);
        QCOMPARE(days.at(0).requests, qint64(1));
        QCOMPARE(days.at(0).tokens.input, qint64(10));
        QCOMPARE(days.at(0).sessions, qint64(1));
    }

    void followsAppendsRewritesAndDeletes() {
        QTemporaryDir dir;
        const QString file = dir.filePath(QStringLiteral("a.jsonl"));
        write(file, entry("2026-10-01T10:00:00Z", "m1", "r1", "s1"));
        TranscriptIndex index;
        index.update({file});
        write(file, entry("2026-10-01T11:00:00Z", "m2", "r2", "s1"), QIODevice::Append);
        index.update({file});
        QCOMPARE(index.days(utc).at(0).requests, qint64(2));
        write(file, entry("2026-10-01T12:00:00Z", "m3", "r3", "s1")); // truncated and rewritten
        index.update({file});
        QCOMPARE(index.days(utc).at(0).requests, qint64(1));
        QVERIFY(QFile::remove(file));
        index.update({});
        QVERIFY(index.days(utc).isEmpty());
    }

    void countsSkippedLines() {
        QTemporaryDir dir;
        const QString file = dir.filePath(QStringLiteral("a.jsonl"));
        write(file,
              QByteArray(R"({"type":"assistant","usage": )") + '\n' + entry("2026-10-01T10:00:00Z", "m1", "r1", "s1"));
        TranscriptIndex index;
        index.update({file});
        QCOMPARE(index.skippedLines(), qint64(1));
        QCOMPARE(index.days(utc).at(0).requests, qint64(1));
    }

    // Review Focus 5: an interruption stops between files.
    void interruptionStopsBetweenFiles() {
        QTemporaryDir dir;
        const QString first = dir.filePath(QStringLiteral("a.jsonl"));
        const QString second = dir.filePath(QStringLiteral("b.jsonl"));
        write(first, entry("2026-10-01T10:00:00Z", "m1", "r1", "s1"));
        write(second, entry("2026-10-01T10:00:00Z", "m2", "r2", "s1"));
        TranscriptIndex index;
        int asked = 0;
        index.update({first, second}, [&asked] { return ++asked > 1; });
        QCOMPARE(index.days(utc).at(0).requests, qint64(1));
    }
};

QTEST_GUILESS_MAIN(TestTranscriptIndex)
#include "tst_transcriptindex.moc"
