#pragma once

#include "core/apppaths.h"
#include "core/snapshot.h"
#include "github/githubaccount.h"

#include "core/sourceschedule.h"
#include <QObject>

namespace contribusage::github {

// FR-10 for GitHub: the last snapshot in state.json under "github", beside the limits.
namespace githubcache {
std::optional<Snapshot<GitHubReport>> load(const AppPaths &paths);
bool save(const AppPaths &paths, const Snapshot<GitHubReport> &snapshot);
// A token change: the old account leaves state.json (ADR-022).
bool clear(const AppPaths &paths);
} // namespace githubcache

// GitHub on an interval (SPEC §12: 30 min) with its cache and SPEC §11.3's states. A 401 pauses it until the token
// changes (ADR-022); a rate limit waits for its reset (ADR-020). A stand-in until the refresh coordinator of L-1c.
class GitHubJob : public QObject {
    Q_OBJECT

public:
    GitHubJob(GitHubAccount &account, AppPaths paths, SchedulePolicy policy,
              ScheduleConditionsSource *conditions = nullptr, QObject *parent = nullptr);

    static SchedulePolicy defaultPolicy(); // SPEC §12 GitHub row: 30 min, 10 min to 6 h, stale 2 h, floor 30 s, network

    const SourceState<GitHubReport> &state() const { return m_state; }

    void start();
    // The manual refresh: runs also while paused (ADR-022), never before a rate limit's reset (ADR-020).
    void refresh();
    // SPEC §12: the popup opened; runs when the data is older than 10 min and the minimum interval has passed.
    void popupOpened();
    // After a connect or a disconnect: drops the old account and any fetch still running with it (ADR-022), ends a
    // pause and fetches now.
    void tokenChanged();
    // Removes the token; a refusal by the secret store shows as a failure and the account stays.
    void disconnectAccount();

Q_SIGNALS:
    void stateChanged();

private:
    void run();
    void finish(std::expected<GitHubReport, SourceError> outcome);

    GitHubAccount &m_account;
    AppPaths m_paths;
    SourceSchedule m_schedule; // paused after a 401, a missing token or a refused wallet; held by a rate limit
    int m_generation = 0;      // bumped by tokenChanged(); a fetch from an older one finishes into nothing
    SourceState<GitHubReport> m_state = SourceState<GitHubReport>::loading(std::nullopt);
};

} // namespace contribusage::github
