---
type: log
updated: 2026-10-30
---
# Log

Append-only record of what changed in the project, newest last. Entry format: `## [YYYY-MM-DD] <type> | <title>` with one to three bullets. `grep '^## \[' log.md | tail -5` shows the latest entries.

## [2026-09-28] wiki | History before 2026-10-20
- Entries for the macOS app and the Linux port up to L-7 were removed in the 2026-10-30 trim; `git log -- llm-wiki/log.md` has them.

## [2026-10-29] decision | No text marker for a stale label value
- ADR-049: the panel label no longer prefixes `~` to a stale value (`23

## [2026-10-29] decision | No text marker for a stale label value
- ADR-049: the panel label no longer prefixes a tilde to a stale value ("23%", "7"); staleness stays in `MenuBarLabel::stale` and the half-strength meter. SPEC US-1 and §11.1 changed; tests updated first.

## [2026-10-29] feat | Themes for the widget: System, macOS, installed Plasma styles
- The Appearance page's Theme replaces Look ([[decisions/0050-widget-themes]], SPEC L-8): macOS draws the macOS app's capsule meters and 2 px corners (stripes included), System uses Plasma's meter image, an installed Plasma style draws its background, meter and colors in this widget only.
- `StyleTint` carries a style's colors to every item, since Kirigami keeps custom colors on the setting item.

## [2026-10-29] feat | A Plasma style restyles the whole popup; macOS draws the app's popover
- `StyleTint` points every Plasma image inside the popup window (frame, meters, controls) at the chosen style's files ([[decisions/0050-widget-themes]]).
- The macOS theme draws its rounded popover card, the app's symbols and label colors, the Insights chevron and 60 % day bars.

## [2026-10-29] feat | Global themes and per-widget icon themes
- The Theme box lists installed global themes: one pick sets the widget's Plasma style, color scheme and icon theme ([[decisions/0050-widget-themes]], SPEC L-8).
- Icons resolve in the chosen icon theme and its inherited themes; macOS takes WhiteSur's icons when installed, else the drawn symbols.

## [2026-10-29] research | Installed themes cannot change the popup's layout
- Checked in the viewer: a global theme changes frame, colors and icons only; layout, fonts and shapes stay the widget's ([[decisions/0050-widget-themes#Consequences]]). `main.qml` and `main.xml` were edited for the check and restored unchanged.

## [2026-10-29] feat | The macOS theme matches the macOS app's popover
- Drawn symbols in the label color, see-through card on the desktop's blur, SF Pro's row height, legend and footer sizes, checked side by side with `docs/assets/screenshot*.png` ([[decisions/0050-widget-themes]]).
- Fixed: the macOS colors were undefined in dark mode (object literal after `?`), so the real popup never got them.

## [2026-10-29] docs | README rewritten for the Linux widget
- Install (makepkg + shell restart), uninstall, build, files and status sections added; macOS-only wording removed; release badge points at this fork.

## [2026-10-30] docs | README images redrawn for Plasma
- `screenshot.png`, `screenshot-dark.png` and `menu-bar-styles.png` show the widget's macOS theme rendered off-screen at 2x from the built applet with the mock data (Breeze Light/Dark palettes), composed with a floating Plasma panel and Noto Sans.
- `social-preview.svg`/`.png` use Noto Sans and carry a "KDE Plasma 6 widget" chip.

## [2026-10-30] fix | CI: kcmutils, root-safe test, clock-independent wiki test
- CI toolchain installs `kcmutils` (the config pages import `org.kde.kcmutils`); `tst_activityjob` skips the unreadable-transcript case when run as root.
- `test-wiki.sh` no longer depends on the UTC date matching the committer's local date.

## [2026-10-30] chore | Trim the repository to a widget's size
- Tests cut from 35 suites to the parsing, persistence, schedule, GitHub client and applet smoke tests; SPEC.md cut to the requirements (macOS-era sections, tasks and appendices removed); implementation plans, Swift-era ADRs, research, sources and templates deleted. Full text: git history and the `macos-reference` tag.
