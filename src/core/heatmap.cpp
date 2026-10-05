#include "core/heatmap.h"

#include <QVariantList>

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

QVariantMap heatmapView(const QList<HeatmapLayer> &layers, QDate today) {
    if (layers.isEmpty()) {
        return {};
    }
    QVariantList layerList;
    for (const HeatmapLayer &layer : layers) {
        layerList.append(QVariantMap{{QStringLiteral("name"), layer.name}, {QStringLiteral("hue"), layer.hue}});
    }
    const QList<QDate> days = heatmapDays(today);
    QVariantList cells;
    for (const QDate &date : days) {
        QString line = date.toString(Qt::ISODate);
        QVariantList stripes;
        for (const HeatmapLayer &layer : layers) {
            line += QStringLiteral(" · ") + layer.name + QStringLiteral(": ") +
                    layer.texts.value(date, QStringLiteral("no data"));
            if (const int level = layer.levels.value(date); level > 0) {
                stripes.append(QVariantMap{{QStringLiteral("hue"), layer.hue}, {QStringLiteral("level"), level}});
            }
        }
        cells.append(QVariantMap{{QStringLiteral("line"), line}, {QStringLiteral("stripes"), stripes}});
    }
    return {{QStringLiteral("layers"), layerList},
            {QStringLiteral("columns"), int((days.size() + 6) / 7)},
            {QStringLiteral("cells"), cells}};
}

} // namespace contribusage
