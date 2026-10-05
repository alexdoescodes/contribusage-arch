#include "platform/networktransport.h"

#include <QEventLoop>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTest>
#include <QTimer>

using namespace contribusage;

namespace {

HttpOutcome post(NetworkTransport &transport, const HttpRequest &request) {
    std::optional<HttpOutcome> outcome;
    QEventLoop loop;
    transport.post(request, nullptr, [&](HttpOutcome result) {
        outcome = std::move(result);
        loop.quit();
    });
    if (!outcome) {
        QTimer::singleShot(10'000, &loop, &QEventLoop::quit);
        loop.exec();
    }
    return outcome.value_or(
        std::unexpected(SourceError{.kind = SourceError::Kind::Io, .text = QStringLiteral("no answer")}));
}

} // namespace

// The live HttpTransport against a local server: what is sent, what comes back, and a dead host.
class TestNetworkTransport : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void postsAndReturnsStatusHeadersAndBody() {
        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost));
        QByteArray received;
        connect(&server, &QTcpServer::newConnection, &server, [&] {
            QTcpSocket *socket = server.nextPendingConnection();
            connect(socket, &QTcpSocket::readyRead, socket, [&received, socket] {
                received += socket->readAll();
                if (received.contains("\r\n\r\n") && received.endsWith("{\"q\":1}")) {
                    socket->write("HTTP/1.1 403 Forbidden\r\nX-RateLimit-Remaining: 0\r\nContent-Length: 2\r\n"
                                  "Connection: close\r\n\r\n{}");
                    socket->disconnectFromHost();
                }
            });
        });
        NetworkTransport transport;
        const HttpOutcome outcome =
            post(transport, {.url = QUrl(QStringLiteral("http://127.0.0.1:%1/graphql").arg(server.serverPort())),
                             .headers = {{"Authorization", "Bearer t"}, {"Content-Type", "application/json"}},
                             .body = "{\"q\":1}"});
        QVERIFY2(outcome, qPrintable(outcome ? QString() : outcome.error().text));
        QCOMPARE(outcome->status, 403);
        QCOMPARE(outcome->headers.value("x-ratelimit-remaining"), QByteArray("0"));
        QCOMPARE(outcome->body, QByteArray("{}"));
        QVERIFY(received.startsWith("POST /graphql HTTP/1.1"));
        QVERIFY(received.contains("Authorization: Bearer t"));
    }

    void unreachableHostIsOffline() {
        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost));
        const quint16 port = server.serverPort();
        server.close(); // nothing listens there any more
        NetworkTransport transport;
        const HttpOutcome outcome =
            post(transport, {.url = QUrl(QStringLiteral("http://127.0.0.1:%1/").arg(port)), .headers = {}, .body = {}});
        QVERIFY(!outcome);
        QCOMPARE(outcome.error().kind, SourceError::Kind::Offline);
    }
};

QTEST_GUILESS_MAIN(TestNetworkTransport)
#include "tst_networktransport.moc"
