#pragma once

#include "core/heatmap.h"
#include "core/reports.h"
#include "core/snapshot.h"

#include <QDate>
#include <QVariantMap>

#include <optional>

namespace contribusage::claudecode {

// US-5, SPEC §11.2: `todayText` ("Today: 668 requests · 8 sessions · 4.2M"), `categoriesText` (the four token
// categories) and `week` (seven bars, oldest first, `fraction` of the busiest day and `today` on the last one). An
// empty map without a snapshot, so QML hides the block.
QVariantMap activityView(const SourceState<ActivityReport> &state, QDate today);

// The Claude Code layer of the heatmap (FR-48): total tokens per day as quartile levels over the shown range, "1.2M
// tokens" as the text; nullopt without a snapshot.
std::optional<HeatmapLayer> activityLayer(const SourceState<ActivityReport> &state, QDate today);

} // namespace contribusage::claudecode
