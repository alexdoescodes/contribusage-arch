#include "core/menubarlabel.h"

#include "core/display.h"

#include <QLocale>

#include <algorithm>

namespace contribusage {

namespace {

bool isReset(const UsageWindow &window, const QDateTime &now) {
    return window.resetsAt && *window.resetsAt <= now;
}

std::optional<double> fraction(const UsageWindow &window, const QDateTime &now) {
    if (isReset(window, now)) {
        return std::nullopt;
    }
    return std::clamp(window.usedPercent / 100.0, 0.0, 1.0);
}

// SPEC §11.1: figure spaces (as wide as a digit) in front up to two digits, so the label ends at its value.
QString padded(const QString &text) {
    const auto digits = std::ranges::count_if(text, [](QChar c) { return c.isDigit(); });
    return QString(QChar(0x2007)).repeated(std::max<qsizetype>(0, 2 - digits)) + text;
}

MenuBarLabel windowLabel(const UsageWindow &window, const MenuBarProvider &provider, const QString &badge,
                         const QDateTime &now) {
    const bool stale = provider.limits && provider.limits->fetchedAt.msecsTo(now) > provider.staleAfterMs;
    const WindowKind otherKind = window.kind == WindowKind::Weekly ? WindowKind::Session : WindowKind::Weekly;
    std::optional<double> companion;
    for (const UsageWindow &other : provider.limits->value.windows) {
        if (other.kind == otherKind && !(other == window)) {
            companion = fraction(other, now);
            break;
        }
    }
    return {.text = padded(QString((stale ? QStringLiteral("~") : QString()) +
                                   percentText(window.usedPercent, window.isBelowOne, window.resetsAt, now))),
            .title = provider.displayName,
            .meter = fraction(window, now),
            .companion = companion,
            .stale = stale,
            .badge = badge,
            .source = provider.id};
}

// The provider's first window of `kind`, else the highest window across providers, else "?" (FR-12, US-1).
MenuBarLabel limitsLabel(std::optional<WindowKind> kind, const QString &providerId,
                         const QList<MenuBarProvider> &providers, const QDateTime &now) {
    const MenuBarProvider *provider = nullptr;
    for (const MenuBarProvider &candidate : providers) {
        if (candidate.id == providerId) {
            provider = &candidate;
        }
    }
    if (kind && provider && provider->limits) {
        for (const UsageWindow &window : provider->limits->value.windows) {
            if (window.kind == *kind) {
                return windowLabel(window, *provider, {}, now);
            }
        }
    }
    const UsageWindow *highest = nullptr;
    const MenuBarProvider *owner = nullptr;
    for (const MenuBarProvider &candidate : providers) {
        if (!candidate.limits) {
            continue;
        }
        for (const UsageWindow &window : candidate.limits->value.windows) {
            if (!isReset(window, now) && (!highest || window.usedPercent > highest->usedPercent)) {
                highest = &window;
                owner = &candidate;
            }
        }
    }
    if (!highest) {
        const MenuBarProvider *shown = provider ? provider : (providers.isEmpty() ? nullptr : &providers.first());
        return {.text = padded(QStringLiteral("?")),
                .title = shown ? shown->displayName : QStringLiteral("contribusage"),
                .meter = std::nullopt,
                .companion = std::nullopt,
                .stale = false,
                .badge = {},
                .source = shown ? shown->id : QString()};
    }
    return windowLabel(*highest, *owner, providers.size() > 1 ? owner->symbol : QString(), now);
}

} // namespace

QList<MenuBarMode> offeredModes(bool providers, bool github) {
    QList<MenuBarMode> modes;
    for (const MenuBarMode mode : {MenuBarMode::Primary, MenuBarMode::Weekly, MenuBarMode::Highest,
                                   MenuBarMode::GithubToday, MenuBarMode::PrimaryAndGitHub, MenuBarMode::IconOnly}) {
        const bool usesProviders = mode == MenuBarMode::Primary || mode == MenuBarMode::Weekly ||
                                   mode == MenuBarMode::Highest || mode == MenuBarMode::PrimaryAndGitHub;
        const bool usesGitHub = mode == MenuBarMode::GithubToday || mode == MenuBarMode::PrimaryAndGitHub;
        if ((providers || !usesProviders) && (github || !usesGitHub)) {
            modes.append(mode);
        }
    }
    return modes;
}

MenuBarMode resolvedMode(MenuBarMode mode, const QList<MenuBarMode> &offered) {
    return offered.contains(mode) ? mode : offered.first();
}

MenuBarLabel menuBarLabel(MenuBarMode mode, const QString &provider, const QList<MenuBarProvider> &providers,
                          const std::optional<MenuBarGitHub> &github, const QDateTime &now) {
    const QString githubText =
        github ? QString((github->stale ? QStringLiteral("~") : QString()) + QLocale().toString(github->today))
               : QStringLiteral("?");
    switch (mode) {
    case MenuBarMode::Primary:
        return limitsLabel(WindowKind::Session, provider, providers, now);
    case MenuBarMode::Weekly:
        return limitsLabel(WindowKind::Weekly, provider, providers, now);
    case MenuBarMode::Highest:
        return limitsLabel(std::nullopt, provider, providers, now);
    case MenuBarMode::GithubToday:
        return {.text = githubText,
                .title = QStringLiteral("GitHub"),
                .meter = github ? std::optional(github->level / 4.0) : std::nullopt,
                .companion = github ? std::optional(github->activeDays / 7.0) : std::nullopt,
                .stale = github && github->stale,
                .badge = {},
                .source = QStringLiteral("github")};
    case MenuBarMode::PrimaryAndGitHub: {
        MenuBarLabel label = limitsLabel(WindowKind::Session, provider, providers, now);
        label.text += QStringLiteral(" · ") + githubText;
        return label;
    }
    case MenuBarMode::IconOnly: {
        if (providers.isEmpty()) {
            return {.text = {},
                    .title = QStringLiteral("contribusage"),
                    .meter = {},
                    .companion = {},
                    .stale = false,
                    .badge = {},
                    .source = {}};
        }
        MenuBarLabel label = limitsLabel(WindowKind::Session, provider, providers, now);
        label.text.clear();
        return label;
    }
    }
    return {};
}

} // namespace contribusage
