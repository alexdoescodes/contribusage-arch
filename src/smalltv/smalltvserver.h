#pragma once

#include "core/snapshot.h"

#include <QHostAddress>
#include <QObject>
#include <QTcpServer>

#include <functional>
#include <optional>

namespace contribusage::smalltv {

// ADR-051: a read-only HTTP endpoint the SmallTV's theme polls. GET / answers 200 with `payload()`, or 503 when the
// limits are missing or stale. Nothing is written, nothing is authenticated, and only the percentages and reset times
// leave the widget. Off unless the user turns it on; the address defaults to loopback.
class SmallTvServer : public QObject {
    Q_OBJECT

public:
    explicit SmallTvServer(QObject *parent = nullptr);

    // Replaces the clock; tests only.
    void setClock(std::function<QDateTime()> now) { m_now = std::move(now); }
    // ADR-052: the folder the Claude Code hooks write session states into; empty: none.
    void setSessionsFolder(QString folder) { m_sessionsFolder = std::move(folder); }
    // The last limits the probe produced (nullopt: none).
    void setSnapshot(std::optional<contribusage::Snapshot<contribusage::LimitsReport>> snapshot);
    // Starts or restarts on `address:port`; false when the socket cannot be opened. Port 0 picks a free one.
    bool start(const QHostAddress &address, quint16 port);
    void stop();
    bool isListening() const { return m_server.isListening(); }
    quint16 port() const { return m_server.serverPort(); }
    QHostAddress address() const { return m_server.serverAddress(); }

private:
    void accept();

    QTcpServer m_server;
    std::optional<contribusage::Snapshot<contribusage::LimitsReport>> m_snapshot;
    std::function<QDateTime()> m_now;
    QString m_sessionsFolder;
};

} // namespace contribusage::smalltv
