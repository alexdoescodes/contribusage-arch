#pragma once

#include "core/seams.h"
#include "github/githubclient.h"
#include "github/githubreport.h"

namespace contribusage::github {

// The saved GitHub token (FR-16): the one owner of its key, and of the rule that only a validated token is saved.
class GitHubAccount {
public:
    GitHubAccount(SecretStore &secrets, GitHubClient &client);

    // FR-17: saves `token` only after GitHub accepted it (SPEC §14) and returns its login.
    void connect(const QString &token, QObject *context, std::function<void(std::expected<QString, SourceError>)> done);

    // FR-18, FR-19: the year up to `now` with its statistics; TokenMissing while no token is saved. The range starts at
    // the start of `now`'s local day 364 days ago (SPEC §8.4.1).
    void report(const QDateTime &now, Qt::DayOfWeek firstDayOfWeek, QObject *context,
                std::function<void(std::expected<GitHubReport, SourceError>)> done);

    void disconnect(QObject *context, std::function<void(SecretWrite)> done);

private:
    SecretStore &m_secrets;
    GitHubClient &m_client;
};

} // namespace contribusage::github
