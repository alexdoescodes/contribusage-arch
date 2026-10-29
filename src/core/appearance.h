#pragma once

#include <QString>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>

namespace contribusage {

// Port design §3, layer 2: token overrides from the `themeTokens` setting (a JSON object). Only known tokens of the
// right kind survive: colors as #rrggbb or #aarrggbb, opacities 0 to 1, sizes clamped to sane ranges.
QVariantMap themeTokensFromText(const QString &json);

// ADR-050: the Plasma styles installed under `<dataDir>/plasma/desktoptheme/` as {id, name}, sorted by name. A folder
// counts when it holds metadata.json or metadata.desktop; the first data folder holding an id wins.
QVariantList plasmaStyles(const QStringList &dataDirs);

// The absolute path of a style's image (`dialogs/background` gives `.../dialogs/background.svgz` or `.svg`), from the
// style or else from Plasma's `default` style; empty when neither has it.
QString plasmaStyleFile(const QStringList &dataDirs, const QString &style, const QString &image);

// A KDE color scheme (a style's `colors` file): textColor, backgroundColor, negative/neutral/positiveTextColor from
// [Colors:Window], highlightColor from [Colors:Selection], each as #rrggbb; unreadable entries are left out.
QVariantMap colorSchemeFromText(const QString &text);

// ADR-050: the global themes (look-and-feel packages) under `<dataDir>/plasma/look-and-feel/` as {id, name}, sorted by
// name. A package counts when it has `contents/defaults`; the first data folder holding an id wins.
QVariantList globalThemes(const QStringList &dataDirs);

// What a global theme sets, from its `contents/defaults`: `style` (the Plasma style, [plasmarc][Theme] name),
// `colorScheme` ([kdeglobals][General] ColorScheme) and `iconTheme` ([kdeglobals][Icons] Theme); empty for an unknown
// id.
QVariantMap globalThemeDefaults(const QStringList &dataDirs, const QString &id);

// ADR-050: the file of an icon in an icon theme (`<dataDir>/icons/<theme>/`), from the directory nearest to 16 px, then
// from the themes it inherits; empty when the theme or the icon is missing.
QString iconFile(const QStringList &dataDirs, const QString &iconTheme, const QString &name);

// Port design §3, ADR-050, from the plasmoid configuration: `theme` ("system", "macOS", "plasma:<style id>" or
// "global:<global theme id>"; an unknown one is "system"; without a theme the old `look` picks: macOS and Custom give
// macOS), `density` (comfortable, compact), `tokens` (the overrides, for every theme), and what the theme draws with:
// `style` (the Plasma style to retarget the popup to; empty for the system's), `meter` (Plasma's meter image: a
// theme-relative path for the system theme, a file for a style, empty for macOS's capsule), `background` (a style's
// dialog background file), `colors` (a color scheme) and `iconTheme` (global themes: theirs; macOS: "WhiteSur").
QVariantMap appearanceFromMap(const QVariantMap &configuration, const QStringList &dataDirs = {});

} // namespace contribusage
