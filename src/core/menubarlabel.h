#pragma once

#include "core/reports.h"
#include "core/snapshot.h"

#include <QDateTime>
#include <QList>
#include <QString>

#include <optional>

namespace contribusage {

// FR-12's display modes (saved as `menuBarMode` in L-5).
enum class MenuBarMode { Primary, Weekly, Highest, GithubToday, PrimaryAndGitHub, IconOnly };

// FR-12: the modes whose sources are on; `IconOnly` always is.
QList<MenuBarMode> offeredModes(bool providers, bool github);
// The mode the label shows: `mode` while offered, else the first offered one.
MenuBarMode resolvedMode(MenuBarMode mode, const QList<MenuBarMode> &offered);

// An enabled provider and its last limits.
struct MenuBarProvider {
    QString id;
    QString displayName;
    QString symbol;
    qint64 staleAfterMs = 0;
    std::optional<Snapshot<LimitsReport>> limits;
};

// GitHub's today: contributions, today's level (0 to 4), this week's days with contributions.
struct MenuBarGitHub {
    int today = 0;
    int level = 0;
    int activeDays = 0;
    bool stale = false;
};

// SPEC §11.1: what the panel label shows.
struct MenuBarLabel {
    QString text;                    // "23%" padded with figure spaces to two digits, "?", "7", or empty for IconOnly
    QString title;                   // the source's name; "contribusage" with every source off
    std::optional<double> meter;     // the shown window's fraction; nullopt without one or while it is reset (FR-11)
    std::optional<double> companion; // the provider's other window; GitHub: active days of 7
    bool stale = false;
    QString badge;  // the provider's symbol when Highest chose among several providers (SPEC §7.7)
    QString source; // a provider ID or "github"; empty with every source off
};

// `mode` is resolved; `providers` are the enabled ones in registry order; `provider` is the label's provider.
MenuBarLabel menuBarLabel(MenuBarMode mode, const QString &provider, const QList<MenuBarProvider> &providers,
                          const std::optional<MenuBarGitHub> &github, const QDateTime &now);

} // namespace contribusage
