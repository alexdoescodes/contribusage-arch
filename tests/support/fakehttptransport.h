#pragma once

#include "core/seams.h"

#include <QPointer>
#include <QTimer>

// Answers every POST with the configured response (or `failure`), on the next event loop pass, and records requests.
class FakeHttpTransport final : public contribusage::HttpTransport {
public:
    void respond(int status, const QByteArray &body, const QMap<QByteArray, QByteArray> &headers = {}) {
        response = {.status = status, .headers = headers, .body = body};
        failure.reset();
    }

    void post(const contribusage::HttpRequest &request, QObject *context,
              std::function<void(contribusage::HttpOutcome)> done) override {
        requests.append(request);
        const contribusage::HttpOutcome outcome =
            failure ? contribusage::HttpOutcome(std::unexpected(*failure)) : contribusage::HttpOutcome(response);
        const QPointer<QObject> guard = context;
        const bool hasContext = context != nullptr;
        QTimer::singleShot(0, [guard, hasContext, outcome, done = std::move(done)]() {
            if (!hasContext || guard) {
                done(outcome);
            }
        });
    }

    contribusage::HttpResponse response{.status = 500, .headers = {}, .body = {}};
    std::optional<contribusage::SourceError> failure;
    QList<contribusage::HttpRequest> requests;
};
