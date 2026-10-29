#include "github/githubclient.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QStringList>

namespace contribusage::github {

namespace {

// Appendix C. restrictedContributionsCount is fetched but not read until R-4 settles its meaning.
const char *const contributionsQuery =
    "query Contributions($from: DateTime!, $to: DateTime!) { viewer { login "
    "contributionsCollection(from: $from, to: $to) { restrictedContributionsCount contributionCalendar { "
    "totalContributions weeks { contributionDays { date contributionCount contributionLevel } } } } } }";

// Appendix C's level names, in level order.
const QStringList levelNames{QStringLiteral("NONE"), QStringLiteral("FIRST_QUARTILE"),
                             QStringLiteral("SECOND_QUARTILE"), QStringLiteral("THIRD_QUARTILE"),
                             QStringLiteral("FOURTH_QUARTILE")};

SourceError decoding(const QString &message) {
    return {.kind = SourceError::Kind::Decoding, .text = message};
}

} // namespace

GitHubClient::GitHubClient(HttpTransport &transport, QString version)
    : m_transport(transport), m_userAgent(QByteArray("contribusage/") + version.toUtf8()) {}

void GitHubClient::viewerLogin(const QString &token, QObject *context,
                               std::function<void(std::expected<QString, SourceError>)> done) {
    send(QStringLiteral("query { viewer { login } }"), {}, token, context,
         [done = std::move(done)](std::expected<QJsonObject, SourceError> data) {
             if (!data) {
                 done(std::unexpected(data.error()));
                 return;
             }
             const QJsonValue login = data->value(QStringLiteral("viewer")).toObject().value(QStringLiteral("login"));
             if (!login.isString()) {
                 done(std::unexpected(decoding(QStringLiteral("No login in response"))));
                 return;
             }
             done(login.toString());
         });
}

void GitHubClient::contributions(const QString &token, const QDateTime &from, const QDateTime &to, QObject *context,
                                 std::function<void(std::expected<ContributionCalendar, SourceError>)> done) {
    const QJsonObject variables{{QStringLiteral("from"), from.toString(Qt::ISODate)},
                                {QStringLiteral("to"), to.toString(Qt::ISODate)}};
    send(QString::fromLatin1(contributionsQuery), variables, token, context,
         [done = std::move(done)](std::expected<QJsonObject, SourceError> data) {
             if (!data) {
                 done(std::unexpected(data.error()));
                 return;
             }
             const QJsonObject viewer = data->value(QStringLiteral("viewer")).toObject();
             const QJsonObject calendar = viewer.value(QStringLiteral("contributionsCollection"))
                                              .toObject()
                                              .value(QStringLiteral("contributionCalendar"))
                                              .toObject();
             if (!viewer.value(QStringLiteral("login")).isString() ||
                 !calendar.value(QStringLiteral("weeks")).isArray()) {
                 done(std::unexpected(decoding(QStringLiteral("No calendar in response"))));
                 return;
             }
             ContributionCalendar result{.login = viewer.value(QStringLiteral("login")).toString(),
                                         .weeks = {},
                                         .totalContributions =
                                             calendar.value(QStringLiteral("totalContributions")).toInt()};
             for (const QJsonValue &weekValue : calendar.value(QStringLiteral("weeks")).toArray()) {
                 QList<ContributionDay> week;
                 for (const QJsonValue &dayValue :
                      weekValue.toObject().value(QStringLiteral("contributionDays")).toArray()) {
                     const QJsonObject day = dayValue.toObject();
                     const QString levelName = day.value(QStringLiteral("contributionLevel")).toString();
                     const qsizetype level = levelNames.indexOf(levelName);
                     if (level < 0) {
                         done(std::unexpected(decoding(QStringLiteral("Unknown contribution level ") + levelName)));
                         return;
                     }
                     week.append({.date = QDate::fromString(day.value(QStringLiteral("date")).toString(), Qt::ISODate),
                                  .count = day.value(QStringLiteral("contributionCount")).toInt(),
                                  .level = int(level)});
                 }
                 result.weeks.append(week);
             }
             done(result);
         });
}

void GitHubClient::send(const QString &query, const QJsonObject &variables, const QString &token, QObject *context,
                        std::function<void(std::expected<QJsonObject, SourceError>)> done) {
    const HttpRequest request{
        .url = graphqlEndpoint(),
        .headers = {{"Authorization", QByteArray("Bearer ") + token.toUtf8()},
                    {"User-Agent", m_userAgent},
                    {"Content-Type", "application/json"}},
        .body = QJsonDocument(QJsonObject{{QStringLiteral("query"), query}, {QStringLiteral("variables"), variables}})
                    .toJson(QJsonDocument::Compact)};
    m_transport.post(request, context, [done = std::move(done)](HttpOutcome outcome) {
        if (!outcome) {
            done(std::unexpected(outcome.error()));
            return;
        }
        const HttpResponse &response = *outcome;
        if (response.status == 401) {
            done(std::unexpected(SourceError{.kind = SourceError::Kind::Unauthorized}));
            return;
        }
        SourceError failure;
        if (response.status >= 200 && response.status < 300) {
            QJsonParseError parseError;
            const QJsonObject envelope = QJsonDocument::fromJson(response.body, &parseError).object();
            if (parseError.error != QJsonParseError::NoError) {
                done(std::unexpected(decoding(parseError.errorString())));
                return;
            }
            const QJsonArray errors = envelope.value(QStringLiteral("errors")).toArray();
            if (envelope.value(QStringLiteral("data")).isObject() && errors.isEmpty()) {
                done(envelope.value(QStringLiteral("data")).toObject());
                return;
            }
            failure =
                decoding(errors.isEmpty() ? QStringLiteral("No data in response")
                                          : errors.first().toObject().value(QStringLiteral("message")).toString());
        } else {
            failure = {.kind = SourceError::Kind::Http, .code = response.status};
        }
        // ADR-020: any failure with an exhausted budget (200 with errors, 403, 429) waits for the reset.
        bool resetOk = false;
        const qint64 reset = response.headers.value("x-ratelimit-reset").toLongLong(&resetOk);
        if (response.headers.value("x-ratelimit-remaining") == "0" && resetOk) {
            done(std::unexpected(
                SourceError{.kind = SourceError::Kind::RateLimited, .until = QDateTime::fromSecsSinceEpoch(reset)}));
            return;
        }
        done(std::unexpected(failure));
    });
}

} // namespace contribusage::github
