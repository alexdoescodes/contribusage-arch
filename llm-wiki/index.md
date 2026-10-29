---
type: index
updated: 2026-10-30
---
# Index

Every page in the vault, one line each; lint checks that each page is listed. The contract is `SPEC.md` (cite by section or ID, `SPEC §8.1.3`, `FR-8`).

- [[overview]]: what contribusage is, how the pieces fit
- [[log]]: chronological record of changes
- [[architecture/repo-map]]: annotated tree of the repository and the vault (lint-checked)

## Modules
- [[modules/applet]]: the Plasma 6 widget; QML views and C++ view models
- [[modules/core]]: provider-neutral models, scheduler, persistence
- [[modules/claude-code]]: the v1 provider; `/usage` probe and transcripts
- [[modules/github]]: GraphQL client, token validation, contribution statistics

## Decisions
- [[decisions/0003-official-interfaces-only]]: ADR-003, no credentials, no undocumented vendor endpoints
- [[decisions/0010-provider-abstraction-from-day-one]]: ADR-010, provider-neutral core, one target per provider
- [[decisions/0041-linux-plasmoid-port]]: ADR-041, Linux-only port as a compiled Plasma widget
- [[decisions/0042-one-week-bar]]: ADR-042, one "Current week" bar; per-model week windows hidden
- [[decisions/0043-inline-github-token-form]]: ADR-043, the GitHub token is entered in the popup and kept in KWallet
- [[decisions/0044-transcripts-on-a-worker-thread]]: ADR-044, transcripts are read on a worker thread
- [[decisions/0045-one-schedule-per-source]]: ADR-045, one schedule object per polled source
- [[decisions/0046-notifications-and-settings]]: ADR-046, freedesktop notifications; settings in the plasmoid configuration
- [[decisions/0047-customization-scope]]: ADR-047, what the customization layers cover
- [[decisions/0048-plasmoid-memory-budget]]: ADR-048, the widget may add 20 MB of private memory
- [[decisions/0050-widget-themes]]: ADR-050, themes (System, macOS, Plasma styles) apply to this widget only

## Research and guides
- [[research/l-7-resource-budget]]: memory, CPU and refresh cost measured against the budget
- [[guides/llm-wiki-tooling]]: the wiki scripts, hooks and lint messages
- [[guides/qml-overrides]]: overriding blocks of the popup from the configuration
