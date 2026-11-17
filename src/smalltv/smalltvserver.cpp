#include "smalltv/smalltvserver.h"

#include "smalltv/smalltvpayload.h"

#include <QPointer>
#include <QTcpSocket>
#include <QTimer>

namespace contribusage::smalltv {

namespace {

constexpr qsizetype maxRequestBytes = 4096;
constexpr int requestTimeoutMs = 2000;

QByteArray response(int status, const QByteArray &reason, const QByteArray &body) {
    return "HTTP/1.1 " + QByteArray::number(status) + ' ' + reason +
           "\r\nContent-Type: application/json\r\nCache-Control: no-store\r\nConnection: close\r\nContent-Length: " +
           QByteArray::number(body.size()) + "\r\n\r\n" + body;
}

} // namespace

SmallTvServer::SmallTvServer(QObject *parent)
    : QObject(parent), m_server(this), m_now([] { return QDateTime::currentDateTimeUtc(); }) {
    connect(&m_server, &QTcpServer::newConnection, this, &SmallTvServer::accept);
}

void SmallTvServer::setSnapshot(std::optional<Snapshot<LimitsReport>> snapshot) {
    m_snapshot = std::move(snapshot);
}

bool SmallTvServer::start(const QHostAddress &address, quint16 port) {
    stop();
    return m_server.listen(address, port);
}

void SmallTvServer::stop() {
    m_server.close();
}

void SmallTvServer::accept() {
    while (QTcpSocket *socket = m_server.nextPendingConnection()) {
        socket->setParent(this);
        connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
        // A client that never finishes its request line is dropped; nothing waits on it.
        QTimer::singleShot(requestTimeoutMs, socket, [socket] { socket->abort(); });
        connect(socket, &QTcpSocket::readyRead, this, [this, socket] {
            const QByteArray request = socket->peek(maxRequestBytes);
            if (!request.contains("\r\n") && request.size() < maxRequestBytes) {
                return;
            }
            const QList<QByteArray> line = request.left(request.indexOf("\r\n")).split(' ');
            QByteArray reply;
            if (line.size() < 2 || line.at(0) != "GET") {
                reply = response(405, "Method Not Allowed", R"({"ok":false})");
            } else if (line.at(1) != "/" && !line.at(1).startsWith("/?")) {
                reply = response(404, "Not Found", R"({"ok":false})");
            } else if (const auto body = payload(m_snapshot, m_now(), readSessions(m_sessionsFolder, m_now()))) {
                reply = response(200, "OK", *body);
            } else {
                reply = response(503, "Service Unavailable", R"({"ok":false})");
            }
            socket->write(reply);
            socket->disconnectFromHost();
        });
    }
}

} // namespace contribusage::smalltv
