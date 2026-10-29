#pragma once

#include "core/seams.h"

#include <QNetworkAccessManager>

namespace contribusage {

// HttpTransport over QNetworkAccessManager: 30 s transfer timeout; no reply from the host at all is Offline.
class NetworkTransport final : public HttpTransport {
public:
    void post(const HttpRequest &request, QObject *context, std::function<void(HttpOutcome)> done) override;

private:
    QNetworkAccessManager m_manager;
};

} // namespace contribusage
