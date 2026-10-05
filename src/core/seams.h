#pragma once

#include "core/reports.h"

#include <QByteArray>
#include <QList>
#include <QMap>
#include <QObject>
#include <QString>
#include <QUrl>

#include <expected>
#include <functional>
#include <optional>
#include <utility>

namespace contribusage {

// SPEC §10.6. Like ProcessRunning, every call is asynchronous, answers once on the GUI thread, and drops its answer
// when `context` was destroyed first. Live implementations live in src/platform/; fakes in tests/support/.

struct HttpRequest {
    QUrl url;
    QList<std::pair<QByteArray, QByteArray>> headers;
    QByteArray body;
};

struct HttpResponse {
    int status = 0;
    QMap<QByteArray, QByteArray> headers; // names in lower case
    QByteArray body;
};

using HttpOutcome = std::expected<HttpResponse, SourceError>;

// A POST; a response with any HTTP status is an outcome, a transport failure is `Offline` (or `TimedOut`).
class HttpTransport {
public:
    virtual ~HttpTransport() = default;
    virtual void post(const HttpRequest &request, QObject *context, std::function<void(HttpOutcome)> done) = 0;
};

using SecretRead = std::expected<std::optional<QString>, SourceError>;
using SecretWrite = std::expected<void, SourceError>;

// Holds only the widget's own secrets (the GitHub token, FR-16), never another tool's (SPEC §2). Removing a missing
// key succeeds.
class SecretStore {
public:
    virtual ~SecretStore() = default;
    virtual void read(const QString &key, QObject *context, std::function<void(SecretRead)> done) = 0;
    virtual void write(const QString &key, const QString &value, QObject *context,
                       std::function<void(SecretWrite)> done) = 0;
    virtual void remove(const QString &key, QObject *context, std::function<void(SecretWrite)> done) = 0;
};

} // namespace contribusage
