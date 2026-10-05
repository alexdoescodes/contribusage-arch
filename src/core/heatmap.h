#pragma once

#include <QDate>
#include <QHash>
#include <QList>
#include <QString>
#include <QVariantMap>

namespace contribusage {

// FR-20, FR-48: 26 week columns from Sunday, as GitHub's, the last one ending with `today`; oldest first.
QList<QDate> heatmapDays(QDate today);

// FR-48: a day's level, 0 to 4: 0 without tokens, otherwise its quartile among the non-zero values, so the busiest
// day is 4. `values` holds the shown range only.
QHash<QDate, int> quartileLevels(const QHash<QDate, qint64> &values);

// One source in the heatmap (FR-20, FR-48): its level per day (0 to 4) and the text of each day it has data for.
struct HeatmapLayer {
    QString name;
    QString hue; // the Theme color key: "claude", "github"
    QHash<QDate, int> levels;
    QHash<QDate, QString> texts;
};

// The heatmap block for QML: `layers` (name, hue), `columns` and one cell per heatmapDays() day with its stripes (one
// per layer with a level above 0, in layer order) and its line ("2026-10-05 · Claude Code: 1.2M tokens · GitHub: 3
// contributions", "no data" for a day a layer has no text for). An empty map without layers, so QML hides the block.
QVariantMap heatmapView(const QList<HeatmapLayer> &layers, QDate today);

} // namespace contribusage
