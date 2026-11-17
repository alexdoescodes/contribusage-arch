#include "smalltv/smalltvpayload.h"
#include "smalltv/smalltvserver.h"

#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTcpSocket>
#include <QTemporaryDir>
#include <QTest>
#include <QTimeZone>

using namespace contribusage;
using namespace contribusage::smalltv;

namespace {

QDateTime utc(int h, int m) {
    return QDateTime(QDate(2026, 11, 17), QTime(h, m), QTimeZone::UTC);
}

Snapshot<LimitsReport> snapshot(const QDateTime &fetchedAt, const QDateTime &now) {
    return Snapshot<LimitsReport>{
        .value = LimitsReport{.provider = QStringLiteral("claude-code"),
                              .windows = {UsageWindow{.label = QStringLiteral("Current session"),
                                                      .kind = WindowKind::Session,
                                                      .usedPercent = 61.6,
                                                      .isBelowOne = false,
                                                      .resetsAt = now.addSecs(95 * 60 + 20)},
                                          UsageWindow{.label = QStringLiteral("Current week (all models)"),
                                                      .kind = WindowKind::Weekly,
                                                      .usedPercent = 38,
                                                      .isBelowOne = false,
                                                      .resetsAt = now.addSecs((2 * 24 * 60 + 23 * 60 + 5) * 60)}},
                              .billingNote = std::nullopt,
                              .insights = std::nullopt,
                              .rawOutput = std::nullopt},
        .fetchedAt = fetchedAt,
        .origin = QStringLiteral("poll")};
}

// Runs the event loop, because the server under test lives in this thread.
QByteArray get(quint16 port, const QByteArray &request) {
    QTcpSocket socket;
    QByteArray all;
    QObject::connect(&socket, &QTcpSocket::connected, [&] { socket.write(request); });
    QObject::connect(&socket, &QTcpSocket::readyRead, [&] { all += socket.readAll(); });
    socket.connectToHost(QHostAddress::LocalHost, port);
    for (int i = 0; i < 300 && socket.state() != QAbstractSocket::UnconnectedState; ++i) {
        QTest::qWait(10);
    }
    return all + socket.readAll();
}

} // namespace

class TstSmallTv : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void formatsResetCountdowns_data() {
        QTest::addColumn<qint64>("minutes");
        QTest::addColumn<QString>("text");
        QTest::newRow("negative") << qint64(-5) << "0m";
        QTest::newRow("zero") << qint64(0) << "0m";
        QTest::newRow("minutes") << qint64(45) << "45m";
        QTest::newRow("59") << qint64(59) << "59m";
        QTest::newRow("exact hour") << qint64(60) << "1h";
        QTest::newRow("hours and minutes") << qint64(95) << "1h 35m";
        QTest::newRow("last minute of the day") << qint64(24 * 60 - 1) << "23h 59m";
        QTest::newRow("exact day") << qint64(24 * 60) << "1d";
        QTest::newRow("days and hours") << qint64(2 * 24 * 60 + 23 * 60 + 5) << "2d 23h";
    }
    void formatsResetCountdowns() {
        QFETCH(qint64, minutes);
        QFETCH(QString, text);
        QCOMPARE(formatReset(minutes), text);
    }

    void payloadCarriesBothWindows() {
        const QDateTime now = utc(12, 0);
        const auto body = payload(snapshot(now.addSecs(-60), now), now);
        QVERIFY(body);
        const QJsonObject json = QJsonDocument::fromJson(*body).object();
        QCOMPARE(json.value(QStringLiteral("s")).toInt(), 62);
        QCOMPARE(json.value(QStringLiteral("sr")).toInt(), 95);
        QCOMPARE(json.value(QStringLiteral("srt")).toString(), QStringLiteral("1h 35m"));
        QCOMPARE(json.value(QStringLiteral("w")).toInt(), 38);
        QCOMPARE(json.value(QStringLiteral("wr")).toInt(), 2 * 24 * 60 + 23 * 60 + 5);
        QCOMPARE(json.value(QStringLiteral("wrt")).toString(), QStringLiteral("2d 23h"));
        QCOMPARE(json.value(QStringLiteral("ok")).toBool(), true);
    }

    void payloadLeavesOutAMissingWindow() {
        const QDateTime now = utc(12, 0);
        auto snap = snapshot(now, now);
        snap.value.windows.removeLast();
        const QJsonObject json = QJsonDocument::fromJson(*payload(snap, now)).object();
        QVERIFY(json.contains(QStringLiteral("s")));
        QVERIFY(!json.contains(QStringLiteral("w")));
        QVERIFY(!json.contains(QStringLiteral("wrt")));
    }

    void noPayloadWithoutFreshData() {
        const QDateTime now = utc(12, 0);
        QVERIFY(!payload(std::nullopt, now));
        QVERIFY(!payload(snapshot(now.addSecs(-staleAfterSeconds - 1), now), now));
        QVERIFY(payload(snapshot(now.addSecs(-staleAfterSeconds), now), now));
        auto none = snapshot(now, now);
        none.value.windows.clear();
        QVERIFY(!payload(none, now));
    }

    void serverAnswersWithThePayload() {
        const QDateTime now = utc(12, 0);
        SmallTvServer server;
        server.setClock([now] { return now; });
        server.setSnapshot(snapshot(now, now));
        QVERIFY(server.start(QHostAddress::LocalHost, 0));
        const QByteArray reply = get(server.port(), "GET / HTTP/1.1\r\nHost: x\r\n\r\n");
        QVERIFY(reply.startsWith("HTTP/1.1 200 OK"));
        QVERIFY(reply.contains("Content-Type: application/json"));
        QVERIFY(reply.contains("\"srt\":\"1h 35m\""));
    }

    void serverAnswers503WhenStale() {
        const QDateTime now = utc(12, 0);
        SmallTvServer server;
        server.setClock([now] { return now.addSecs(staleAfterSeconds + 60); });
        server.setSnapshot(snapshot(now, now));
        QVERIFY(server.start(QHostAddress::LocalHost, 0));
        QVERIFY(get(server.port(), "GET / HTTP/1.1\r\n\r\n").startsWith("HTTP/1.1 503"));
        server.setSnapshot(std::nullopt);
        QVERIFY(get(server.port(), "GET / HTTP/1.1\r\n\r\n").startsWith("HTTP/1.1 503"));
    }

    void serverRejectsOtherPathsAndMethods() {
        SmallTvServer server;
        QVERIFY(server.start(QHostAddress::LocalHost, 0));
        QVERIFY(get(server.port(), "GET /secrets HTTP/1.1\r\n\r\n").startsWith("HTTP/1.1 404"));
        QVERIFY(get(server.port(), "POST / HTTP/1.1\r\n\r\n").startsWith("HTTP/1.1 405"));
    }

    void countsSessionStates() {
        const QDateTime now = utc(12, 0);
        QTemporaryDir dir;
        const auto write = [&](const QString &name, const QByteArray &text, const QDateTime &when) {
            QFile file(dir.filePath(name));
            QVERIFY(file.open(QIODevice::WriteOnly));
            file.write(text);
            QVERIFY(file.flush());
            QVERIFY(file.setFileTime(when, QFileDevice::FileModificationTime));
        };
        write(QStringLiteral("a"), "waiting\n", now.addSecs(-60));
        write(QStringLiteral("b"), "waiting\n", now.addSecs(-sessionExpirySeconds + 60));
        write(QStringLiteral("c"), "running\n", now.addSecs(-5));
        write(QStringLiteral("old"), "waiting\n", now.addSecs(-sessionExpirySeconds - 60));
        write(QStringLiteral("junk"), "hello", now);
        QVERIFY(QDir(dir.path()).mkdir(QStringLiteral("sub")));
        const SessionCounts counts = readSessions(dir.path(), now);
        QCOMPARE(counts.waiting, 2);
        QCOMPARE(counts.running, 1);
        QCOMPARE(readSessions(dir.filePath(QStringLiteral("missing")), now).waiting, 0);
        QCOMPARE(readSessions(QString(), now).running, 0);
    }

    void payloadCarriesSessionCounts() {
        const QDateTime now = utc(12, 0);
        const QJsonObject json =
            QJsonDocument::fromJson(*payload(snapshot(now, now), now, SessionCounts{.waiting = 2, .running = 1}))
                .object();
        QCOMPARE(json.value(QStringLiteral("a")).toInt(), 2);
        QCOMPARE(json.value(QStringLiteral("r")).toInt(), 1);
        const QJsonObject none = QJsonDocument::fromJson(*payload(snapshot(now, now), now)).object();
        QCOMPARE(none.value(QStringLiteral("a")).toInt(-1), 0);
    }

    void serverReadsTheSessionsFolder() {
        const QDateTime now = QDateTime::currentDateTimeUtc();
        QTemporaryDir dir;
        QFile file(dir.filePath(QStringLiteral("s1")));
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("waiting\n");
        file.close();
        SmallTvServer server;
        server.setSessionsFolder(dir.path());
        server.setSnapshot(snapshot(now, now));
        QVERIFY(server.start(QHostAddress::LocalHost, 0));
        QVERIFY(get(server.port(), "GET / HTTP/1.1\r\n\r\n").contains("\"a\":1"));
        QVERIFY(file.remove());
        QVERIFY(get(server.port(), "GET / HTTP/1.1\r\n\r\n").contains("\"a\":0"));
    }

    void stopClosesThePort() {
        SmallTvServer server;
        QVERIFY(server.start(QHostAddress::LocalHost, 0));
        const quint16 port = server.port();
        server.stop();
        QVERIFY(!server.isListening());
        QVERIFY(get(port, "GET / HTTP/1.1\r\n\r\n").isEmpty());
    }
};

QTEST_GUILESS_MAIN(TstSmallTv)
#include "tst_smalltv.moc"
