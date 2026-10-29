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
    if (state.phase == Phase::NotConfigured && state.reason == NotConfiguredReason::ProviderDisabled) {
        return {}; // FR-43: off hides the block
    }
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
        view.insert(QStringLiteral("skeleton"), true);
    } else if (state.phase == Phase::Failed) {
        view.insert(QStringLiteral("statusText"), errorMessage(*state.error, QStringLiteral("GitHub")));
        if (state.error->kind != SourceError::Kind::Unauthorized &&
            state.error->kind != SourceError::Kind::TokenMissing) {
            view.insert(QStringLiteral("action"), QVariantMap{{QStringLiteral("kind"), QStringLiteral("retry")},
                                                              {QStringLiteral("text"), QStringLiteral("Retry")}});
        }
    }
    return view;
}

std::optional<HeatmapLayer> githubLayer(const SourceState<GitHubReport> &state, const QDateTime &now) {
    if (state.phase == SourceState<GitHubReport>::Phase::NotConfigured &&
        state.reason == NotConfiguredReason::ProviderDisabled) {
        return std::nullopt;
    }
    const auto snapshot = state.snapshot();
    if (!snapshot) {
        return std::nullopt;
    }
    HeatmapLayer layer{.name = QStringLiteral("GitHub"), .hue = QStringLiteral("github"), .levels = {}, .texts = {}};
    for (const ContributionDay &day : snapshot->value.calendar.days()) {
        layer.levels.insert(day.date, day.level);
        layer.texts.insert(day.date, QString::number(day.count) + QStringLiteral(" contributions"));
    }
    layer.dimmed = snapshot->isStale(now, staleAfterSeconds) || state.phase == SourceState<GitHubReport>::Phase::Failed;
    return layer;
}

QVariantMap githubHeatmap(const SourceState<GitHubReport> &state, QDate today) {
    const auto layer = githubLayer(state, QDateTime::currentDateTime());
    return layer ? heatmapView({*layer}, today) : QVariantMap();
}

std::optional<MenuBarGitHub> githubMenuBar(const SourceState<GitHubReport> &state, QDate today,
                                           Qt::DayOfWeek firstDayOfWeek, const QDateTime &now) {
    const auto snapshot = state.snapshot();
    if (!snapshot) {
        return std::nullopt;
    }
    MenuBarGitHub data;
    const QDate weekStart = today.addDays(-((today.dayOfWeek() - int(firstDayOfWeek) + 7) % 7));
    for (const ContributionDay &day : snapshot->value.calendar.days()) {
        if (day.date == today) {
            data.today = day.count;
            data.level = day.level;
        }
        if (day.date >= weekStart && day.date <= today && day.count > 0) {
            ++data.activeDays;
        }
    }
    data.stale = snapshot->isStale(now, staleAfterSeconds) || state.phase == SourceState<GitHubReport>::Phase::Failed;
    return data;
}

} // namespace contribusage::github
