---
type: decision
status: accepted
updated: 2026-10-29
aliases: [ADR-047]
tracks: [src/applet/Theme.qml, src/applet/OverridableBlock.qml, src/core/appearance.cpp]
tags: [customization, ui]
---
# ADR-047: What the customization layers cover

- **Decided:** 2026-10-07 · **Spec:** port design §3 (customization), SPEC §11.4, §17.8 L-6 · **Supersedes:** —
- **Note (2026-10-29):** the looks are replaced by themes in [[decisions/0050-widget-themes]]; tokens, fonts, assets and overrides stand as decided here.

## Context
The port design lists for its three customization layers: looks macOS, Plasma and Custom; tokens for every color, font, size, spacing, radius and palette, plus density, popup width and label size; QML overrides of each block with "documented, versioned properties"; Inter as the default font and bundled SVG symbols. L-6 built the layers; some of the listed items were left out, and that needs a record rather than silence.

## Options
| Option | For | Against |
|---|---|---|
| Build every listed item now | Matches the design word for word | Font and symbol files to ship and license; radius and font tokens touch every component for little visible gain |
| Build the layers with the tokens the components use, record the rest | Small, tested; nothing hidden | Fonts, radii and the label size are not tokens yet |

## Decision
- **Looks:** macOS (the defaults), Plasma (Kirigami's highlight, neutral, negative and positive colors for accent, warning and the Claude Code hue, critical, and the GitHub hue), Custom (the defaults with validated token overrides).
- **Tokens:** the colors, opacities and sizes the components read today (`appearance.cpp` lists them), the density and the popup width. Fonts, corner radii and the label size are not tokens: the widget uses the system font (Plasma's), the radii stay fixed, the label follows the panel's icon size.
- **No bundled assets:** no Inter font and no SVG symbol set; Breeze icon names stand in for SF Symbols.
- **Overrides:** `ProviderGroup`, `HeatmapBlock` and `GitHubSection`, through `OverridableBlock`. The properties are contract version 1 in [[guides/qml-overrides]]; a later change to them bumps the version there.

## Consequences
- A font, radius or label-size token can be added later as another validated entry in `themeTokensFromText` and a binding in `Theme.qml`.
- Packaging (L-7) ships only the plugin.
