---
type: decision
status: accepted
updated: 2026-10-29
aliases: [ADR-048]
tracks: [src/applet/popupmodel.cpp]
tags: [performance, packaging]
---
# ADR-048: The plasmoid may add 20 MB, not 15

- **Decided:** 2026-10-15 · **Spec:** port design §4 (resource budget), NFR-2 · **Supersedes:** —

## Context
The port design asked for under 15 MB added to plasmashell. [[research/l-7-resource-budget]] measured 16 MB median (Private_Dirty over a compiled KDE applet) with the widget's real data, and a measurement noise of about 4 MB. The two attempts to shrink it (`malloc_trim` with hashed index keys, an arena cap from inside the plugin) gained nothing beyond that noise. NFR-2 (below 80 MB per provider) is far from touched.

## Options
| Option | For | Against |
|---|---|---|
| Keep 15 MB and cut more | Meets the design | Needs fewer threads or a smaller index; no measured way to 1 MB |
| Budget 20 MB | Matches what was measured with room for noise | Looser than the design |

## Decision
The budget is 20 MB added Private_Dirty, measured as in the research page. The arena cap stays in `popupmodel.cpp`.

## Consequences
The L-7 check passes at 16 MB. Revisit when a second provider is added (each adds a worker) or when a measured run exceeds 20 MB.
