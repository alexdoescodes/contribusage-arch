#pragma once

#include "core/apppaths.h"
#include "core/snapshot.h"
#include "github/githubaccount.h"

#include <QObject>
#include <QTimer>

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
    GitHubJob(GitHubAccount &account, AppPaths paths, int intervalMs, QObject *parent = nullptr);

    const SourceState<GitHubReport> &state() const { return m_state; }

    void start();
    void refresh();
    // After a connect or a disconnect: drops the old account and any fetch still running with it (ADR-022), ends a
    // pause and fetches now.
    void tokenChanged();
    // Removes the token; a refusal by the secret store shows as a failure and the account stays.
    void disconnectAccount();

Q_SIGNALS:
    void stateChanged();

private:
    void finish(std::expected<GitHubReport, SourceError> outcome);

    GitHubAccount &m_account;
    AppPaths m_paths;
    int m_interval;
    QTimer m_timer;
    bool m_fetching = false;
    bool m_paused = false; // a 401, a missing token or a refused wallet: nothing to ask until the token changes
    int m_generation = 0;  // bumped by tokenChanged(); a fetch from an older one finishes into nothing
    QDateTime m_waitUntil;
    SourceState<GitHubReport> m_state = SourceState<GitHubReport>::loading(std::nullopt);
};

} // namespace contribusage::github
