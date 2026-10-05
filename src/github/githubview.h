#pragma once

#include "core/snapshot.h"
#include "github/githubreport.h"

#include <QDate>
#include <QDateTime>
#include <QVariantMap>

namespace contribusage::github {

// What QML reads for the GitHub block: login ("@octocat"), statsText, updatedText, statusText, connected (false while
// no token is saved: the block shows the token form) and stale.
QVariantMap githubView(const SourceState<GitHubReport> &state, const QDateTime &now);

// FR-20: GitHub's layer in L-4a's heatmap shape, over the 26 weeks up to `today`; empty without a snapshot.
QVariantMap githubHeatmap(const SourceState<GitHubReport> &state, QDate today);

} // namespace contribusage::github
