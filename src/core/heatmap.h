#pragma once

#include <QDate>
#include <QHash>
#include <QList>

namespace contribusage {

// FR-20, FR-48: 26 week columns from Sunday, as GitHub's, the last one ending with `today`; oldest first.
QList<QDate> heatmapDays(QDate today);

// FR-48: a day's level, 0 to 4: 0 without tokens, otherwise its quartile among the non-zero values, so the busiest
// day is 4. `values` holds the shown range only.
QHash<QDate, int> quartileLevels(const QHash<QDate, qint64> &values);

} // namespace contribusage
