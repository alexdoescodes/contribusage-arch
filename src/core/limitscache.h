#pragma once

#include "core/apppaths.h"
#include "core/reports.h"
#include "core/snapshot.h"

#include <optional>

namespace contribusage::limitscache {

// state.json: {"limits": {"<provider>": {"fetchedAt": …, "origin": …, "report": …}}} (SPEC §10.7). Other keys (the
// GitHub snapshot, notification keys) join it in later milestones and are kept on save.
inline constexpr int schemaVersion = 1;

// FR-10: the last successful limits snapshot; nullopt when there is none or the cache is unreadable.
std::optional<Snapshot<LimitsReport>> load(const AppPaths &paths, const QString &providerId);

// Replaces this provider's snapshot and keeps everything else; an unreadable cache is started afresh.
bool save(const AppPaths &paths, const Snapshot<LimitsReport> &snapshot);
// US-12: drops a provider's cached limits, keeping the file's other entries.
bool remove(const AppPaths &paths, const QString &providerId);

} // namespace contribusage::limitscache
