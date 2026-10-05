#include "claudecode/activityview.h"

#include "core/display.h"

#include <QVariantList>

namespace contribusage::claudecode {

namespace {
QVariantMap retryAction() {
    return {{QStringLiteral("kind"), QStringLiteral("retry")}, {QStringLiteral("text"), QStringLiteral("Retry")}};
}
} // namespace

QVariantMap activityView(const SourceState<ActivityReport> &state, QDate today) {
    const auto snapshot = state.snapshot();
    if (!snapshot) {
        if (state.phase == SourceState<ActivityReport>::Phase::NotConfigured) {
            return {{QStringLiteral("statusText"), QStringLiteral("No Claude Code sessions found on this machine")}};
        }
        if (state.phase == SourceState<ActivityReport>::Phase::Failed) {
            return {{QStringLiteral("statusText"), errorMessage(*state.error, QStringLiteral("Claude Code"))},
                    {QStringLiteral("action"), retryAction()}};
        }
        return {{QStringLiteral("skeleton"), true}};
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
    QVariantMap view{
        {QStringLiteral("todayText"),
         QString(QStringLiteral("Today: ") + QString::number(now.requests) + QStringLiteral(" requests · ") +
                 QString::number(now.sessions) + QStringLiteral(" sessions · ") + compactCount(now.tokens.total()))},
        {QStringLiteral("categoriesText"),
         QString(QStringLiteral("Input ") + compactCount(now.tokens.input) + QStringLiteral(" · Output ") +
                 compactCount(now.tokens.output) + QStringLiteral(" · Cache read ") +
                 compactCount(now.tokens.cacheRead) + QStringLiteral(" · Cache write ") +
                 compactCount(now.tokens.cacheWrite))},
        {QStringLiteral("week"), week}};
    // SPEC §11.3: a failed rescan keeps the previous numbers, dimmed, with its error and Retry.
    if (state.phase == SourceState<ActivityReport>::Phase::Failed) {
        view.insert(QStringLiteral("stale"), true);
        view.insert(QStringLiteral("statusText"), errorMessage(*state.error, QStringLiteral("Claude Code")));
        view.insert(QStringLiteral("action"), retryAction());
    }
    return view;
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
