---
type: decision
status: accepted
updated: 2026-10-29
aliases: [ADR-050]
tracks: [src/applet/styletint.h, src/applet/popupmodel.cpp, src/applet/symbols, src/applet/Footer.qml, src/applet/InsightsSection.qml, src/applet/SectionHeader.qml, src/applet/ActivitySection.qml, src/applet/Theme.qml, src/applet/FullPopup.qml, src/applet/LimitBar.qml, src/applet/HeatmapBlock.qml, src/applet/styletint.cpp, src/applet/ConfigAppearance.qml, src/core/appearance.cpp]
tags: [customization, ui]
---
# ADR-050: Themes replace looks; installed global themes and Plasma styles apply to the widget only

- **Decided:** 2026-10-29 · **Spec:** SPEC §11 · **Supersedes:** the looks of [[decisions/0047-customization-scope#Decision]]

## Context
The three looks of [[decisions/0047-customization-scope]] differed in six colors only; the popup looked the same in each. The Appearance page needs a theme that changes the look itself: one that matches the macOS app (`macos-reference`, `App/Popover/*.swift`), and the Plasma styles installed on the system, chosen for this widget without changing the desktop's style.

The popup's frame is not the widget's to draw: plasmashell's `CompactApplet.qml` creates the `AppletPopup` window, and its background always comes from the global Plasma style. Kirigami keeps a custom color on the item that sets it (`Kirigami.Theme.textColor` set on the popup does not reach the labels inside it).

## Options
| Option | For | Against |
|---|---|---|
| Keep the looks, add more tokens | Small | Still colors and sizes only; no installed style |
| Themes: system, macOS, each installed Plasma style, drawn by the widget | One choice changes shapes, meter, background and colors; the desktop stays as it is | The style's background is drawn inside a window whose frame the shell draws; Kirigami colors need a walker |
| Switch the global Plasma style while the popup is open | Exact | Changes every widget and panel; not "this widget only" |

## Decision
- **Theme** (config key `theme`) replaces **Look**: `system` (the default: Plasma's colors and Plasma's meter image `widgets/bar_meter_horizontal`, square cells and bars), `macOS` (the macOS app's colors and shapes: SwiftUI's capsule meter, `cornerRadius(2)` on heatmap cells, their stripes, the 7-day bars and the legend swatches), `global:<id>` for each global theme under `<data dir>/plasma/look-and-feel/<id>/` holding `contents/defaults`, or `plasma:<id>` for each style under `<data dir>/plasma/desktoptheme/<id>/` holding `metadata.json` or `metadata.desktop`.
- **A Plasma style** gives the popup its `dialogs/background` (drawn over the whole popup window, so it covers the frame the shell draws), its meter image, and its `colors` file (window text and background, selection, negative/neutral/positive). A missing image comes from Plasma's `default` style, as in Plasma; a style without colors keeps Plasma's.
- **A global theme** is one pick for the whole look: from its `contents/defaults` the Plasma style (`[plasmarc][Theme] name`, applied as above; `default` leaves the frame to the system), the color scheme (`[kdeglobals][General] ColorScheme`, read from `<data dir>/color-schemes/<name>.colors`, over the style's colors) and the icon theme (`[kdeglobals][Icons] Theme`). Its widget style (Kvantum, Breeze) is a Qt Widgets style and does not apply to QML.
- **Icons** come from the theme's icon theme, for this widget only: `iconFile` reads `<data dir>/icons/<theme>/index.theme`, takes the directory nearest to 16 px (Fixed, Scalable, Threshold), then searches the inherited themes (at most eight, cycles ended); a name the chain lacks keeps the desktop's icon. Files are cached per theme and name.
- **Colors reach every item** through `StyleTint`, which sets the style's colors on each item's Kirigami theme in the popup, including blocks created later.
- **Every Plasma image follows the style:** `StyleTint` also rewrites the theme-relative `imagePath` of each KSvg item under its target (`widgets/button`, `widgets/tabbar`, `dialogs/background`, …) to the chosen style's file, keeping the relative path to restore it. Inside a Plasma popup window the target is the window's content item, so the window's own frame is restyled too; Plasma's controls stay, drawn from another folder.
- **macOS** matches the app's popover in `docs/assets/screenshot.png` and `screenshot-dark.png`: its own rounded, see-through card (the window's dialog background is hidden through `StyleTint.hideFrame`, so the card lies on the desktop's blur, whose region the hidden frame's mask still sets), the app's label colors for light and dark, its symbols as small SVGs drawn for the widget (`src/applet/symbols/`; sparkle, calendar, grid, arrow, gear, documents, chevrons; icons without a drawing come from WhiteSur when installed), labels 1 px tighter above and below (Noto Sans lines are 17 px where SF Pro's are about 15), a 10/6 px legend, 22 px footer symbols, the Insights chevron, and 7-day bars at 60 % of each day's width.
- **Tokens** apply on top of every theme. An unset `theme` takes the old `look` (macOS and Custom give `macOS`, everything else `system`).
- Fonts stay global (no theme carries one); a Plasma style alone keeps the desktop's icons; the window's shadow stays the desktop style's.

## Consequences
- A new widget follows the system; one that saved Custom or macOS keeps the macOS colors.
- The window's blur region and shadow follow the global style.
- Kirigami's mask coloring in Plasma fills a symbol's outlines and cut-outs solid (a calendar drawn as a stroked rectangle showed as a white square); the drawn symbols are therefore written, already in the label color, to `~/.cache/contribusage/symbols/<name>-<hash of the drawing>-<color>.svg` (a changed drawing gets a new file) and drawn as plain files. Icon theme files are drawn as they are, without the mask.
- Text's `lineHeight` does not shrink a single line; the tighter macOS rows use negative label padding instead.
- In Plasma's engine an object literal written bare after `?` in a property binding evaluated to undefined (the macOS colors in dark mode); each color set is its own property.
- A warning or critical meter in a Plasma theme is a capsule in its color over the style's track: the style's fill image has one color.
- A global theme or Plasma style changes the frame, meter and control images, the colors and the icons only: the popup's layout, fonts, spacing and shapes are the widget's own and stay the same in every installed theme (checked with Edna-Light and WhiteSur-dark, 2026-10-29). Themes that change the whole look need per-theme layout definitions in the widget.
- Revisit if plasmashell lets an applet set its popup's style.
