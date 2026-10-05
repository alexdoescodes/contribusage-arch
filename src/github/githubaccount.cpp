#include "github/githubaccount.h"

#include <QTimeZone>

namespace contribusage::github {

namespace {
const QString tokenKey = QStringLiteral("token");
}

GitHubAccount::GitHubAccount(SecretStore &secrets, GitHubClient &client) : m_secrets(secrets), m_client(client) {}

void GitHubAccount::connect(const QString &token, QObject *context,
                            std::function<void(std::expected<QString, SourceError>)> done) {
    m_client.viewerLogin(token, context, [this, token, context, done = std::move(done)](auto login) mutable {
        if (!login) {
            done(std::unexpected(login.error()));
            return;
        }
        m_secrets.write(tokenKey, token, context, [login = *login, done = std::move(done)](SecretWrite written) {
            if (!written) {
                done(std::unexpected(written.error()));
                return;
            }
            done(login);
        });
    });
}

void GitHubAccount::report(const QDateTime &now, Qt::DayOfWeek firstDayOfWeek, QObject *context,
                           std::function<void(std::expected<GitHubReport, SourceError>)> done) {
    m_secrets.read(
        tokenKey, context, [this, now, firstDayOfWeek, context, done = std::move(done)](SecretRead token) mutable {
            if (!token) {
                done(std::unexpected(token.error()));
                return;
            }
            if (!token->has_value()) {
                done(std::unexpected(SourceError{.kind = SourceError::Kind::TokenMissing}));
                return;
            }
            const QDateTime from(now.date().addDays(-364), QTime(0, 0), now.timeZone());
            m_client.contributions(
                **token, from, now, context, [now, firstDayOfWeek, done = std::move(done)](auto calendar) {
                    if (!calendar) {
                        done(std::unexpected(calendar.error()));
                        return;
                    }
                    done(GitHubReport{.calendar = *calendar,
                                      .stats = contributionStats(calendar->days(), now.date(), firstDayOfWeek)});
                });
        });
}

void GitHubAccount::disconnect(QObject *context, std::function<void(SecretWrite)> done) {
    m_secrets.remove(tokenKey, context, std::move(done));
}

} // namespace contribusage::github
