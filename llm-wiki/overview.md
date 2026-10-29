---
type: overview
status: active
updated: 2026-10-30
tracks: []
tags: [hub]
---
# contribusage

A KDE Plasma 6 widget for Arch Linux (C++23, Qt 6, QML) that shows how much of an AI coding plan is left and when it resets (v1: Claude Code), coding activity on this machine, and the GitHub contribution graph. It uses only official interfaces and never touches another tool's credentials ([[decisions/0003-official-interfaces-only]]). It is the Linux port of a macOS app ([[decisions/0041-linux-plasmoid-port]]); the Swift code is at the git tag `macos-reference` (commit `2603a21`).

- **`SPEC.md`: the contract.** Requirements with stable IDs.
- **This vault: the memory.** How things are built and why. Latest changes: [[log]].
- **`AGENTS.md`: the protocol** for agents working in the repository.

## Targets
Dependency direction: `contribusage_claudecode → contribusage_core ← contribusage_github`; the applet depends on all of them and is the only place providers are registered.

| Target | Page | Role |
|---|---|---|
| applet | [[modules/applet]] | Plasma widget: QML views, C++ view models |
| core | [[modules/core]] | Provider-neutral models, scheduling, persistence |
| claudecode | [[modules/claude-code]] | The v1 provider: `/usage` probe and transcripts |
| github | [[modules/github]] | Contribution calendar and statistics |
