#include "claudecode/activityview.h"

#include "core/display.h"

#include <QVariantList>

namespace contribusage::claudecode {

QVariantMap activityView(const SourceState<ActivityReport> &state, QDate today) {
    const auto snapshot = state.snapshot();
    if (!snapshot) {
        return {};
    }
    QHash<QDate, ActivityDay> byDay;
    for (const ActivityDay &day : snapshot->value.days) {
        byDay.insert(day.day, day);
    }
    const ActivityDay now = byDay.value(today);
    QList<qint64> totals;
    for (int offset = 6; offset >= 0; --offset) {
        totals.append(byDay.value(today.addDays(-offset)).tokens.total());
    }
    const QList<double> fractions = barFractions(totals);
    QVariantList week;
    for (qsizetype index = 0; index < fractions.size(); ++index) {
        week.append(QVariantMap{{QStringLiteral("fraction"), fractions.at(index)},
                                {QStringLiteral("today"), index == fractions.size() - 1}});
    }
    return {{QStringLiteral("todayText"), QString(QStringLiteral("Today: ") + QString::number(now.requests) +
                                                  QStringLiteral(" requests · ") + QString::number(now.sessions) +
                                                  QStringLiteral(" sessions · ") + compactCount(now.tokens.total()))},
            {QStringLiteral("categoriesText"),
             QString(QStringLiteral("Input ") + compactCount(now.tokens.input) + QStringLiteral(" · Output ") +
                     compactCount(now.tokens.output) + QStringLiteral(" · Cache read ") +
                     compactCount(now.tokens.cacheRead) + QStringLiteral(" · Cache write ") +
                     compactCount(now.tokens.cacheWrite))},
            {QStringLiteral("week"), week}};
}

std::optional<HeatmapLayer> activityLayer(const SourceState<ActivityReport> &state, QDate today) {
    const auto snapshot = state.snapshot();
    if (!snapshot) {
        return std::nullopt;
    }
    const QList<QDate> shown = heatmapDays(today);
    const QDate first = shown.first();
    HeatmapLayer layer{
        .name = QStringLiteral("Claude Code"), .hue = QStringLiteral("claude"), .levels = {}, .texts = {}};
    QHash<QDate, qint64> values;
    for (const ActivityDay &day : snapshot->value.days) {
        if (day.day >= first && day.day <= today) {
            values.insert(day.day, day.tokens.total());
            layer.texts.insert(day.day, compactCount(day.tokens.total()) + QStringLiteral(" tokens"));
        }
    }
    layer.levels = quartileLevels(values);
    return layer;
}

} // namespace contribusage::claudecode
