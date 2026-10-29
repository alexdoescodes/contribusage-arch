#include "platform/networktransport.h"

#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPointer>

namespace contribusage {

void NetworkTransport::post(const HttpRequest &request, QObject *context, std::function<void(HttpOutcome)> done) {
    QNetworkRequest networkRequest(request.url);
    for (const auto &[name, value] : request.headers) {
        networkRequest.setRawHeader(name, value);
    }
    networkRequest.setTransferTimeout(30'000);
    QNetworkReply *reply = m_manager.post(networkRequest, request.body);
    const QPointer<QObject> guard = context;
    const bool hasContext = context != nullptr;
    QObject::connect(reply, &QNetworkReply::finished, reply, [reply, guard, hasContext, done = std::move(done)] {
        reply->deleteLater();
        if (hasContext && !guard) {
            return;
        }
        const QVariant status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute);
        if (!status.isValid()) {
            // No HTTP answer: DNS, connection or timeout failures.
            done(std::unexpected(SourceError{.kind = reply->error() == QNetworkReply::OperationCanceledError ||
                                                             reply->error() == QNetworkReply::TimeoutError
                                                         ? SourceError::Kind::TimedOut
                                                         : SourceError::Kind::Offline,
                                             .text = reply->errorString()}));
            return;
        }
        HttpResponse response{.status = status.toInt(), .headers = {}, .body = reply->readAll()};
        for (const auto &[name, value] : reply->rawHeaderPairs()) {
            response.headers.insert(name.toLower(), value);
        }
        done(response);
    });
}

} // namespace contribusage
