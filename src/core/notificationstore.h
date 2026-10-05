#pragma once

#include "core/apppaths.h"
#include "core/notificationplanner.h"

namespace contribusage::notificationstore {

// FR-15: the planner's cycles under `notifications` in state.json; empty when missing or unreadable.
NotificationsSent load(const AppPaths &paths);
// Keeps the file's other entries.
bool save(const AppPaths &paths, const NotificationsSent &sent);

} // namespace contribusage::notificationstore
