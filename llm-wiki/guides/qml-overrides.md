---
type: guide
status: active
updated: 2026-10-05
tracks: [src/applet/FullPopup.qml, src/applet/Theme.qml]
tags: [customization, qml]
---
# QML overrides

The third customization layer of the port design (§3): a QML file of your own replaces one of the popup's blocks. The widget loads it in place of the built-in block and falls back to the built-in block when it fails.

## Details

- **Where:** `~/.local/share/contribusage/qml/<Block>.qml` (`$XDG_DATA_HOME/contribusage/qml`; Settings → Appearance shows the folder and opens it).
- **Blocks and what they receive** (the same properties as the built-in block, kept up to date while the widget runs):

| File | Property | Content |
|---|---|---|
| `ProviderGroup.qml` | `provider` | The Claude Code map: `name`, `symbol`, `updatedText`, `statusText`, `stale`, `skeleton`, `action`, `rawOutput`, `windows` (each `label`, `percentText`, `fraction`, `level`, `resetText`), `activity`, `insights`, `sections` |
| `HeatmapBlock.qml` | `heatmap` | `layers` (`name`, `hue`, `dimmed`), `columns`, `cells` (`line`, `stripes`: `hue`, `level`, `dimmed`), `style` (`combined` or `stacked`) |
| `GitHubSection.qml` | `github` | `login`, `statsText`, `updatedText`, `statusText`, `connected`, `needsToken`, `stale`, `skeleton`, `action`, `busy`, `connectError` |

- **Theme:** the `Theme` singleton of the module (`import plasma.applet.org.contribusage.widget`) holds the tokens: colors `accent`, `warning`, `critical`, `claudeHue`, `githubHue`; opacities `secondaryOpacity`, `trackOpacity`, `emptyCellOpacity`, `dividerOpacity`, `mutedBarOpacity`; sizes `popupWidth`, `padding`, `spacing`, `meterHeight`, `cellGap`, `weekChartHeight`; and `meterColor(level)`, `hue(name)`, `levelOpacity(level)`.
- **Fallback:** a file that does not load (a syntax error, a missing import) is skipped; the built-in block shows with the line "Custom <Block> failed to load; showing the built-in one." The widget reads the folder when the popup is built, so a fixed file shows after the widget restarts.
- **Security:** an override is code that runs inside plasmashell with your rights. Use only files you wrote or trust.

## Related

- [[modules/applet]]
