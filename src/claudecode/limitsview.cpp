#include "claudecode/limitsview.h"

#include "claudecode/limitsjob.h"
#include "claudecode/usageparser.h"
#include "core/menubarlabel.h"

#include "core/display.h"

#include <QVariantList>

namespace contribusage::claudecode {

namespace {

constexpr qint64 staleAfterSeconds = 30 * 60; // SPEC §8.1.4

QVariantMap windowView(const UsageWindow &window, const QString &label, const QDateTime &now) {
    return {
        {QStringLiteral("label"), label},
        {QStringLiteral("percentText"), percentText(window.usedPercent, window.isBelowOne, window.resetsAt, now)},
        {QStringLiteral("fraction"), barFraction(window.usedPercent, window.resetsAt, now)},
        {QStringLiteral("level"), meterLevelName(meterLevel(window.usedPercent, window.resetsAt, now))},
        {QStringLiteral("resetText"), resetText(window.resetsAt, now).value_or(QString())},
    };
}

QVariantList rows(const QList<Share> &shares, qsizetype limit) {
    QVariantList list;
    for (const Share &share : shares.first(std::min(limit, shares.size()))) {
        list.append(QVariantMap{
            {QStringLiteral("label"), share.label},
            {QStringLiteral("percent"), share.percent ? QString(QString::number(*share.percent) + u'%') : QString()}});
    }
    return list;
}

QVariantMap insightsView(const Insights &insights) {
    QVariantList periods;
    for (const InsightsPeriod &period : insights.periods) {
        QVariantList rankings;
        for (const Ranking &ranking : period.rankings) {
            rankings.append(QVariantMap{{QStringLiteral("title"), ranking.title},
                                        {QStringLiteral("rows"), rows(ranking.items, 3)}});
        }
        periods.append(QVariantMap{{QStringLiteral("label"), period.label},
                                   {QStringLiteral("summary"), period.summary},
                                   {QStringLiteral("rows"), rows(period.shares, period.shares.size())},
                                   {QStringLiteral("rankings"), rankings}});
    }
    return {{QStringLiteral("note"), insights.note.value_or(QString())}, {QStringLiteral("periods"), periods}};
}

QString statusText(const SourceState<LimitsReport> &state) {
    using Phase = SourceState<LimitsReport>::Phase;
    switch (state.phase) {
    case Phase::Loading:
        return state.snapshot() ? QString() : QStringLiteral("Loading…");
    case Phase::NotConfigured:
        return state.reason == NotConfiguredReason::UnsupportedPlan
                   ? QStringLiteral("Plan limits are only available when Claude Code uses a Claude subscription")
                   : QStringLiteral("Claude Code not found");
    case Phase::Failed:
        return errorMessage(*state.error, QStringLiteral("Claude Code"));
    case Phase::Loaded:
        break;
    }
    return {};
}

} // namespace

QVariantMap limitsView(const SourceState<LimitsReport> &state, const QDateTime &now) {
    QVariantList windows;
    QString updated;
    bool stale = false;
    if (const auto snapshot = state.snapshot()) {
        for (const UsageWindow &window : snapshot->value.windows) {
            if (const auto label = shownWindowLabel(window.label)) {
                windows.append(windowView(window, *label, now));
            }
        }
        updated = ageText(snapshot->fetchedAt, now);
        stale = snapshot->isStale(now, staleAfterSeconds) || state.phase == SourceState<LimitsReport>::Phase::Failed;
    }
    QVariantMap view{{QStringLiteral("name"), QStringLiteral("Claude Code")},
                     {QStringLiteral("symbol"), QStringLiteral("draw-star")},
                     {QStringLiteral("updatedText"), updated},
                     {QStringLiteral("statusText"), statusText(state)},
                     {QStringLiteral("stale"), stale},
                     {QStringLiteral("windows"), windows}};
    if (const auto snapshot = state.snapshot(); snapshot && snapshot->value.insights) {
        view.insert(QStringLiteral("insights"), insightsView(*snapshot->value.insights));
    }
    using Phase = SourceState<LimitsReport>::Phase;
    view.insert(QStringLiteral("skeleton"), state.phase == Phase::Loading && !state.snapshot());
    const auto action = [&view](const QString &kind, const QString &text) {
        view.insert(QStringLiteral("action"),
                    QVariantMap{{QStringLiteral("kind"), kind}, {QStringLiteral("text"), text}});
    };
    if (state.phase == Phase::NotConfigured && state.reason == NotConfiguredReason::ToolNotInstalled) {
        action(QStringLiteral("configure"), QStringLiteral("Locate…"));
    } else if (state.phase == Phase::Failed && state.error->kind == SourceError::Kind::Unparseable) {
        action(QStringLiteral("raw"), QStringLiteral("Show raw output"));
        view.insert(QStringLiteral("rawOutput"), state.error->text);
    } else if (state.phase == Phase::Failed) {
        action(QStringLiteral("retry"), QStringLiteral("Retry"));
    }
    return view;
}

QVariantMap labelView(const SourceState<LimitsReport> &state, const QDateTime &now, MenuBarMode mode) {
    const MenuBarProvider provider{.id = providerId(),
                                   .displayName = QStringLiteral("Claude Code"),
                                   .symbol = QStringLiteral("draw-star"),
                                   .staleAfterMs = LimitsJob::defaultPolicy().staleAfterMs,
                                   .limits = state.snapshot()};
    const MenuBarLabel label = menuBarLabel(mode, provider.id, {provider}, std::nullopt, now);
    const double meter = label.meter.value_or(0.0);
    return {{QStringLiteral("text"), label.text},
            {QStringLiteral("fraction"), meter},
            {QStringLiteral("level"), meterLevelName(meterLevel(meter * 100))},
            {QStringLiteral("stale"), label.stale},
            {QStringLiteral("title"), label.title},
            {QStringLiteral("companion"), label.companion.value_or(-1.0)},
            {QStringLiteral("source"), label.source}};
}

} // namespace contribusage::claudecode
