#include "claudecode/transcriptline.h"

#include <QTest>

using namespace contribusage;
using namespace contribusage::claudecode;

namespace {
// SPEC Appendix E, with the IDs and path scrubbed.
const QByteArray assistant =
    R"({"type":"assistant","isSidechain":false,"sessionId":"s-1","cwd":"/home/octocat/code","timestamp":"2026-09-27T12:34:56.789Z","requestId":"req_1","message":{"id":"msg_1","model":"claude-opus-5-5","role":"assistant","usage":{"input_tokens":12,"cache_creation_input_tokens":1834,"cache_read_input_tokens":145210,"output_tokens":421}}})";
} // namespace

// SPEC §8.3.2, FR-23, ported from TranscriptLineTests.swift.
class TestTranscriptLine : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void decodesAssistantLine() {
        const auto decoded = decodeTranscriptLine(assistant);
        QVERIFY(decoded && decoded->has_value());
        const TranscriptLine &line = **decoded;
        QCOMPARE(line.timestamp, QDateTime(QDate(2026, 9, 27), QTime(12, 34, 56, 789), QTimeZone::UTC));
        QCOMPARE(line.sessionId, QStringLiteral("s-1"));
        QCOMPARE(line.model, QStringLiteral("claude-opus-5-5"));
        QCOMPARE(line.isSidechain, false);
        QCOMPARE(line.tokens, (TokenCounts{.input = 12, .output = 421, .cacheWrite = 1834, .cacheRead = 145210}));
        QCOMPARE(line.key(), QStringLiteral("msg_1:req_1"));
    }

    void missingOptionalFieldsDefault() {
        const auto decoded = decodeTranscriptLine(
            R"({"type":"assistant","timestamp":"2026-09-27T12:34:56Z","message":{"model":"m","usage":{"output_tokens":5}}})");
        QVERIFY(decoded && decoded->has_value());
        QCOMPARE((*decoded)->tokens, (TokenCounts{.input = 0, .output = 5, .cacheWrite = 0, .cacheRead = 0}));
        QCOMPARE((*decoded)->key(), QString());
        QCOMPARE((*decoded)->sessionId, QString());
    }

    void keyFallsBackToTheMessageId() {
        const auto decoded = decodeTranscriptLine(
            R"({"type":"assistant","timestamp":"2026-09-27T12:34:56Z","message":{"id":"msg_9","model":"m","usage":{}}})");
        QVERIFY(decoded && decoded->has_value());
        QCOMPARE((*decoded)->key(), QStringLiteral("msg_9"));
    }

    void skipsLinesWithoutUsage_data() {
        QTest::addColumn<QByteArray>("line");
        QTest::newRow("user") << QByteArray(R"({"type":"user","timestamp":"2026-09-27T12:00:00Z","message":{}})");
        QTest::newRow("no usage") << QByteArray(
            R"({"type":"assistant","timestamp":"2026-09-27T12:00:00Z","message":{"model":"m"}})");
        QTest::newRow("synthetic") << QByteArray(
            R"({"type":"assistant","timestamp":"2026-09-27T12:00:00Z","message":{"model":"<synthetic>","usage":{}}})");
        QTest::newRow("summary with usage word") << QByteArray(R"({"type":"summary","summary":"usage"})");
    }
    void skipsLinesWithoutUsage() {
        QFETCH(QByteArray, line);
        const auto decoded = decodeTranscriptLine(line);
        QVERIFY(decoded);
        QVERIFY(!decoded->has_value());
    }

    void malformedLinesFail_data() {
        QTest::addColumn<QByteArray>("line");
        QTest::newRow("not JSON") << QByteArray(R"({"type":"assistant","usage": )");
        QTest::newRow("no timestamp") << QByteArray(
            R"({"type":"assistant","message":{"model":"m","usage":{"input_tokens":1}}})");
        QTest::newRow("bad timestamp") << QByteArray(
            R"({"type":"assistant","timestamp":"yesterday","message":{"model":"m","usage":{}}})");
    }
    void malformedLinesFail() {
        QFETCH(QByteArray, line);
        QVERIFY(!decodeTranscriptLine(line));
    }
};

QTEST_GUILESS_MAIN(TestTranscriptLine)
#include "tst_transcriptline.moc"
