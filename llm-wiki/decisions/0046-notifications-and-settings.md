---
type: decision
status: accepted
updated: 2026-10-29
aliases: [ADR-046]
tracks: [src/platform/desktopnotifier.cpp, src/core/widgetsettings.cpp, src/applet/main.qml]
tags: [notifications, settings, platform]
---
# ADR-046: Notifications over the freedesktop service; settings in the plasmoid configuration

- **Decided:** 2026-10-05 · **Spec:** FR-13 to FR-15, FR-52, SPEC §10.7, §11.6 · **Supersedes:** —

## Context
The macOS app delivered notifications through UserNotifications and kept settings in UserDefaults. A Plasma widget has KNotification and KConfig-backed plasmoid configuration; KNotification needs a `.notifyrc` installed with the package, which does not exist before L-7.

## Options
| Option | For | Against |
|---|---|---|
| KNotification | KDE's own API, per-event settings in System Settings | Needs an installed notifyrc; silent without it |
| `org.freedesktop.Notifications` over D-Bus | Works from the build tree and in any session; Plasma's server shows it | No per-event configuration in System Settings |
| Settings in a file of our own | Full control | Duplicates what Plasma already stores per widget |

## Decision
The platform's `DesktopNotifier` calls `Notify` asynchronously and replaces a window cycle's earlier note by its server id (FR-52). Settings are entries of the widget's `config/main.xml`; `main.qml` hands `Plasmoid.configuration` to `PopupModel.applySettings`, and `widgetSettingsFromMap` validates them in the core.

## Consequences
- Moving to KNotification later only swaps the sink once the package installs a notifyrc (L-7).
- Each widget instance has its own settings, as Plasma does for every widget.
