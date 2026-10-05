#include "github/githubview.h"

#include "core/display.h"
#include "core/heatmap.h"

#include <QHash>
#include <QLocale>
#include <QVariantList>

namespace contribusage::github {

namespace {
constexpr qint64 staleAfterSeconds = 2 * 3600; // SPEC §12 GitHub row
}

QVariantMap githubView(const SourceState<GitHubReport> &state, const QDateTime &now) {
    using Phase = SourceState<GitHubReport>::Phase;
    QVariantMap view{{QStringLiteral("login"), QString()},
                     {QStringLiteral("statsText"), QString()},
                     {QStringLiteral("updatedText"), QString()},
                     {QStringLiteral("statusText"), QString()},
                     {QStringLiteral("connected"), state.phase != Phase::NotConfigured},
                     {QStringLiteral("stale"), false},
                     // ADR-043: the token form shows while no token is saved and after GitHub rejected it.
                     {QStringLiteral("needsToken"),
                      state.phase == Phase::NotConfigured ||
                          (state.phase == Phase::Failed && state.error->kind == SourceError::Kind::Unauthorized)}};
    if (const auto snapshot = state.snapshot()) {
        const GitHubReport &report = snapshot->value;
        view.insert(QStringLiteral("login"), QString(u'@' + report.calendar.login));
        view.insert(QStringLiteral("statsText"),
                    QString(QStringLiteral("Today ") + QString::number(report.stats.today) +
                            QStringLiteral(" · Streak ") + QString::number(report.stats.currentStreak) +
                            QStringLiteral(" days · Year ") + QLocale().toString(report.calendar.totalContributions)));
        view.insert(QStringLiteral("updatedText"), ageText(snapshot->fetchedAt, now));
        view.insert(QStringLiteral("stale"), snapshot->isStale(now, staleAfterSeconds) || state.phase == Phase::Failed);
    }
    if (state.phase == Phase::Loading && !state.snapshot()) {
        view.insert(QStringLiteral("statusText"), QStringLiteral("Loading…"));
    } else if (state.phase == Phase::Failed) {
        view.insert(QStringLiteral("statusText"), errorMessage(*state.error, QStringLiteral("GitHub")));
    }
    return view;
}

std::optional<HeatmapLayer> githubLayer(const SourceState<GitHubReport> &state) {
    const auto snapshot = state.snapshot();
    if (!snapshot) {
        return std::nullopt;
    }
    HeatmapLayer layer{.name = QStringLiteral("GitHub"), .hue = QStringLiteral("github"), .levels = {}, .texts = {}};
    for (const ContributionDay &day : snapshot->value.calendar.days()) {
        layer.levels.insert(day.date, day.level);
        layer.texts.insert(day.date, QString::number(day.count) + QStringLiteral(" contributions"));
    }
    return layer;
}

QVariantMap githubHeatmap(const SourceState<GitHubReport> &state, QDate today) {
    const auto layer = githubLayer(state);
    return layer ? heatmapView({*layer}, today) : QVariantMap();
}

} // namespace contribusage::github
