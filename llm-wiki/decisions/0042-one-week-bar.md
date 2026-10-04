---
type: decision
status: accepted
updated: 2026-10-05
aliases: [ADR-042]
tracks: [src/core/display.cpp, src/claudecode/limitsview.cpp, src/applet/popupmodel.cpp]
tags: [ui, limits]
---
# ADR-042: One week bar in the popup

- **Decided:** 2026-10-04 · **Spec:** P-4, SPEC §11.2 · **Supersedes:** the display part of [[decisions/0033-provider-tool-texts]] for week windows

## Context
`/usage` prints the weekly limit for all models and, on some plans, a week window per model (`Current week (Opus)`). The popup showed each as its own bar, one below the other. A second "Current week" bar with a model name reads as noise next to the all-models one.

## Options
| Option | For | Against |
|---|---|---|
| Rename only the model bar | Keeps every value | Two bars both read "Current week" |
| Strip model names from every label | Short labels | Two identical labels, no way to tell them apart |
| One week bar: the all-models window as "Current week", model windows hidden | One clear week figure, as the label and the notifications think of the week | A model's own limit is no longer visible in the popup |

## Decision
The popup shows one week bar. `contribusage::shownWindowLabel` maps `Current week (all models)` to `Current week` and hides any other `Current week (…)` window; every other window keeps its printed label (FR-8). Parsing and persistence keep every window.

## Consequences
- A model's own week limit is not shown, and it sends no notifications either (decided 2026-10-04): L-5's notification planning skips every window `shownWindowLabel` hides. Whether Copy follows the rule is still open.
- Revisit if a plan ever prints only model windows and no all-models window: the popup would then show no week bar.
