---
type: decision
status: accepted
updated: 2026-10-29
aliases: [ADR-049]
tags: [label]
tracks: [src/core/menubarlabel.cpp, src/core/menubarlabel.h]
---
# ADR-049: No text marker for a stale value in the panel label

- **Decided:** 2026-10-29 · **Spec:** SPEC §11.1, US-1 · **Supersedes:** —

## Context
The macOS port prefixed a stale value with `~` (`~23%`, `~7`). In the small panel label the tilde sits next to the glyph and reads as visual noise.

## Options
| Option | For | Against |
|---|---|---|
| Keep `~` | Stale is readable as text, also without color or meter detail | Looks cluttered beside the small icon |
| Drop the marker, keep the half-strength meter | Clean label; the meter already shows staleness | Staleness is only visible as dimming; the GitHub count has no meter in every style |

## Decision
The label text is the same for a fresh and a stale value. Staleness stays in `MenuBarLabel::stale`, which the views draw as the meter at half strength; the popover still says "updated N min ago".

## Consequences
A stale `githubToday` count looks like a fresh one except for the glyph's dimming. Revisit if users miss the text cue.
