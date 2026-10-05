#include "core/jsonlreader.h"

#include <QFile>
#include <QTemporaryDir>
#include <QTest>

using namespace contribusage;
using Outcome = IncrementalJsonlReader::Outcome;

namespace {

void write(const QString &path, const QByteArray &bytes, QIODevice::OpenMode mode = QIODevice::WriteOnly) {
    QFile file(path);
    QVERIFY(file.open(mode));
    QCOMPARE(file.write(bytes), bytes.size());
}

struct Read {
    std::expected<Outcome, QString> outcome;
    QList<QByteArray> lines;
};

Read read(IncrementalJsonlReader &reader, const QString &path) {
    Read result{.outcome = Outcome::Gone, .lines = {}};
    result.outcome = reader.read(path, [&result](QByteArrayView line) { result.lines.append(line.toByteArray()); });
    return result;
}

} // namespace

// SPEC §8.3.5, ported from IncrementalJSONLReaderTests.swift.
class TestJsonlReader : public QObject {
    Q_OBJECT

private Q_SLOTS:
    // Review Focus 1: a partial last line waits for its newline and is handed over once.
    void readsOnlyAppendedCompleteLines() {
        QTemporaryDir dir;
        const QString path = dir.filePath(QStringLiteral("a.jsonl"));
        write(path, "one\ntwo\nthr");
        IncrementalJsonlReader reader;
        const Read first = read(reader, path);
        QCOMPARE(first.outcome, Outcome::Appended);
        QCOMPARE(first.lines, (QList<QByteArray>{"one", "two"}));
        write(path, "ee\nfour\n", QIODevice::Append);
        const Read second = read(reader, path);
        QCOMPARE(second.outcome, Outcome::Appended);
        QCOMPARE(second.lines, (QList<QByteArray>{"three", "four"}));
        QCOMPARE(read(reader, path).lines, QList<QByteArray>{});
    }

    void emptyLinesAreLeftOut() {
        QTemporaryDir dir;
        const QString path = dir.filePath(QStringLiteral("a.jsonl"));
        write(path, "a\n\n\nb\n");
        IncrementalJsonlReader reader;
        QCOMPARE(read(reader, path).lines, (QList<QByteArray>{"a", "b"}));
    }

    void rescansAShrunkFile() {
        QTemporaryDir dir;
        const QString path = dir.filePath(QStringLiteral("a.jsonl"));
        write(path, "one\ntwo\n");
        IncrementalJsonlReader reader;
        read(reader, path);
        write(path, "new\n"); // truncates
        const Read again = read(reader, path);
        QCOMPARE(again.outcome, Outcome::Rescanned);
        QCOMPARE(again.lines, QList<QByteArray>{"new"});
    }

    void rescansAReplacedFile() {
        QTemporaryDir dir;
        const QString path = dir.filePath(QStringLiteral("a.jsonl"));
        write(path, "one\n");
        IncrementalJsonlReader reader;
        read(reader, path);
        const QString other = dir.filePath(QStringLiteral("b.jsonl"));
        write(other, "one\ntwo\nthree\n");
        QVERIFY(QFile::remove(path));
        QVERIFY(QFile::rename(other, path));
        const Read again = read(reader, path);
        QCOMPARE(again.outcome, Outcome::Rescanned);
        QCOMPARE(again.lines, (QList<QByteArray>{"one", "two", "three"}));
    }

    void deletedFileReadsGone() {
        QTemporaryDir dir;
        const QString path = dir.filePath(QStringLiteral("a.jsonl"));
        write(path, "one\n");
        IncrementalJsonlReader reader;
        read(reader, path);
        QVERIFY(QFile::remove(path));
        QCOMPARE(read(reader, path).outcome, Outcome::Gone);
        write(path, "one\n");
        const Read again = read(reader, path);
        QCOMPARE(again.outcome, Outcome::Appended); // a new file: nothing to drop
        QCOMPARE(again.lines, QList<QByteArray>{"one"});
    }

    void readsLinesAcrossBufferBoundaries() {
        QTemporaryDir dir;
        const QString path = dir.filePath(QStringLiteral("a.jsonl"));
        const QByteArray longLine(200'000, 'x');
        write(path, longLine + "\nshort\n");
        IncrementalJsonlReader reader;
        const Read result = read(reader, path);
        QCOMPARE(result.lines.size(), qsizetype(2));
        QCOMPARE(result.lines.at(0), longLine);
        QCOMPARE(result.lines.at(1), QByteArray("short"));
    }
};

QTEST_GUILESS_MAIN(TestJsonlReader)
#include "tst_jsonlreader.moc"
