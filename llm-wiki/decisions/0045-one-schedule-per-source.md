---
type: decision
status: accepted
updated: 2026-10-05
aliases: [ADR-045]
tracks: [src/core/sourceschedule.cpp, src/core/schedule.cpp]
tags: [scheduling, architecture]
---
# ADR-045: One schedule object per polled source

- **Decided:** 2026-10-05 · **Spec:** SPEC §12, ADR-018, ADR-022 · **Supersedes:** —

## Context
The macOS app had one `RefreshCoordinator` actor that polled every source, consumed pushes and planned notifications (ADR-018). In the Plasma widget each source already is a job object with its own cache and state, the process runner already serialises child processes (NFR-18), and Qt timers are cheap.

## Options
| Option | For | Against |
|---|---|---|
| One coordinator object | Mirrors the Swift design | Re-creates per-source state inside a second object; more code between the jobs and their timer |
| A `SourceSchedule` per job, fed by shared conditions | Each job owns its whole life cycle; the pure `nextRun` stays the single decision | Cross-source ordering relies on the process runner's queue |

## Decision
Every polled job owns a `SourceSchedule` that arms one timer from `nextRun` (SPEC §12 rule 7) and emits `due()`. The machine's conditions live in one `ScheduleConditionsSource` that the platform's `SystemConditions` feeds from logind and QNetworkInformation. Rule 5's registry order is the process runner's FIFO.

## Consequences
- Notification planning hooks into the limits job when delivery arrives (L-5).
- Low Power Mode stays off until power-profiles-daemon is read (L-5).
