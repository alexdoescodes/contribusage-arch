#include "core/heatmap.h"

#include <algorithm>

namespace contribusage {

QList<QDate> heatmapDays(QDate today) {
    const int weekdayFromSunday = today.dayOfWeek() % 7 + 1; // Qt counts Monday 1 to Sunday 7
    const int count = 25 * 7 + weekdayFromSunday;
    QList<QDate> days;
    days.reserve(count);
    for (int offset = count - 1; offset >= 0; --offset) {
        days.append(today.addDays(-offset));
    }
    return days;
}

QHash<QDate, int> quartileLevels(const QHash<QDate, qint64> &values) {
    QList<qint64> active;
    for (const qint64 value : values) {
        if (value > 0) {
            active.append(value);
        }
    }
    std::sort(active.begin(), active.end());
    QHash<QDate, int> levels;
    for (auto it = values.cbegin(); it != values.cend(); ++it) {
        if (it.value() <= 0) {
            levels.insert(it.key(), 0);
            continue;
        }
        const qsizetype atMost = std::upper_bound(active.cbegin(), active.cend(), it.value()) - active.cbegin();
        levels.insert(it.key(), int((4 * atMost + active.size() - 1) / active.size()));
    }
    return levels;
}

} // namespace contribusage
