#pragma once

#include "core/menubarlabel.h"
#include "core/reports.h"
#include "core/snapshot.h"

#include <QDateTime>
#include <QVariantMap>

namespace contribusage::claudecode {

// What QML reads for the Claude Code group (L-4a's shape): name, symbol, updatedText, statusText, stale, windows
// (ADR-042 applied) and, when the report has them, insights with each ranking cut to its top three (FR-38).
QVariantMap limitsView(const SourceState<LimitsReport> &state, const QDateTime &now);

// SPEC §11.1: the session window's text, fraction and level for the panel label; "?" without one.
QVariantMap labelView(const SourceState<LimitsReport> &state, const QDateTime &now,
                      MenuBarMode mode = MenuBarMode::Primary, std::optional<MenuBarGitHub> github = std::nullopt);

} // namespace contribusage::claudecode
