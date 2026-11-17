---
type: decision
status: accepted
updated: 2026-11-17
aliases: [ADR-051]
tracks: [src/smalltv, src/applet/popupmodel.cpp, src/applet/Footer.qml, src/applet/ConfigSmallTV.qml, src/applet/config/main.xml, src/core/widgetsettings.cpp]
tags: [network, smalltv]
---
# ADR-051: An opt-in HTTP endpoint serves the limits to a SmallTV clock

- **Decided:** 2026-11-17 · **Spec:** FR-53, §1.3, §2 principle 3, §14 Network

## Context
A SmallTV desk clock can show the Claude Code limits. Its firmware fetches small JSON documents from a URL, and its theme format cannot do arithmetic, so a reset time in minutes cannot become "2d 23h" on the clock. A separate daemon would run a second probe of the same limits. The widget already has them, but the spec said "no backend server" and named `api.github.com` as the only network destination.

## Options
| Option | For | Against |
|---|---|---|
| A separate daemon that reads `state.json` | The widget and the spec stay as they are | A second process to install and keep running; a second reader of a file the widget owns |
| The widget pushes to the clock | No listening socket | Needs the clock's address; the theme cannot read what the firmware's usage screen receives |
| **The widget serves JSON, opt-in** | One probe, no extra process, the theme polls it | The widget opens a socket, so the spec's no-server rule needs an exception |

## Decision
- The widget serves `GET /` from a `QTcpServer` in `src/smalltv/`. Setting `smalltvEnabled` is off by default; the address defaults to `127.0.0.1`, so enabling it alone exposes nothing. A LAN address has to be typed.
- The body is built from the last limits snapshot with the clock's own splitting of reset times (`formatReset`). Limits older than 15 minutes, or none, give `503`, which the clock treats as a failed fetch and so falls back to its clock face.
- It answers only `GET /` (`404`, `405` otherwise), reads at most 4 KiB of a request, drops a client that stays silent for 2 s and closes every connection after one answer. Nothing it serves is a secret; there is no authentication, and the settings page says so.
- It starts and stops from the settings and the footer switch, and never runs a probe or a timer of its own.
- The spec's no-server statements gain this one named exception instead of being rewritten.

## Consequences
- The widget is a listener while the switch is on: any host that can reach the address can read two percentages and two reset times.
- The clock sees data only while Plasma runs the widget; otherwise it gets a refused connection and shows its clock face.
- A port that is taken or an address that does not parse leaves the endpoint closed and shows the reason on the SmallTV page.
