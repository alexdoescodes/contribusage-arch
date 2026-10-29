#pragma once

#include "core/seams.h"
#include "github/githubreport.h"

#include <QDateTime>
#include <QUrl>

namespace contribusage::github {

// SPEC §2 principle 3, §8.4.1, NFR-13: the only network destination.
inline QUrl graphqlEndpoint() {
    return QUrl(QStringLiteral("https://api.github.com/graphql"));
}

// GraphQL over HttpTransport (SPEC §8.4). The token is passed per call and never logged. The caller keeps the client
// alive while a call runs.
class GitHubClient {
public:
    GitHubClient(HttpTransport &transport, QString version);

    // FR-17: the login the token belongs to.
    void viewerLogin(const QString &token, QObject *context,
                     std::function<void(std::expected<QString, SourceError>)> done);

    // FR-18: the calendar from `from` to `to` (Appendix C; GitHub accepts at most one year). Both are sent as their
    // local time with its offset.
    void contributions(const QString &token, const QDateTime &from, const QDateTime &to, QObject *context,
                       std::function<void(std::expected<ContributionCalendar, SourceError>)> done);

private:
    // One query; its `data`, or the failure of SPEC §8.4.3.
    void send(const QString &query, const QJsonObject &variables, const QString &token, QObject *context,
              std::function<void(std::expected<QJsonObject, SourceError>)> done);

    HttpTransport &m_transport;
    QByteArray m_userAgent;
};

} // namespace contribusage::github
