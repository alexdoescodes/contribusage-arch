---
type: decision
status: accepted
updated: 2026-10-30
aliases: [ADR-041]
tracks: [CMakeLists.txt, src]
tags: [platform, architecture]
---
# ADR-041: Linux-only port as a KDE Plasma widget

- **Decided:** 2026-10-04 · **Spec:** SPEC header, §1.3, §2, §17.8 · **Supersedes:** the Swift/SwiftUI, macOS 14 and Apple Silicon decisions of the macOS app (git tag `macos-reference`)

## Context
This fork targets Arch Linux on KDE Plasma 6 (Wayland) and drops macOS. The goal is the least use of system resources, the macOS look by default, and a customizable appearance. The earlier design documents were removed on 2026-10-30; this page and SPEC.md carry what still holds.

## Options
| Option | For | Against |
|---|---|---|
| C++/Qt logic library inside a compiled plasmoid | No extra process, no IPC, no second runtime; everything in the Arch repos | About 3,000 lines of logic rewritten; a crash in our code takes down plasmashell |
| Rust daemon and a QML plasmoid over D-Bus | Crash isolation, small memory | A second process, a D-Bus API to maintain, two languages |
| Swift daemon reusing `ContribusageKit` | Logic and tests kept | About 1 GB toolchain, about 30 MB runtime, Foundation gaps on Linux, no mature D-Bus library |

## Decision
A compiled Plasma 6 applet, `org.contribusage.widget`, built with `plasma_add_applet`, links a static C++23 library `contribusage_core` that holds all logic. The Swift tests and fixtures are ported one-to-one to Qt Test; the Swift code stays at the git tag `macos-reference` (commit `2603a21` on `main`).

## Consequences
- macOS is no longer built or maintained here. ADR-001, ADR-008 and ADR-009 no longer apply to this fork; the other ADRs keep their design and are re-linked to C++ files as each milestone lands.
- The probe's own cost (R-1) dominates the resource budget, so its interval rules (ADR-039) stay.
- Revisit if plasmashell stability suffers from the in-process logic: the Rust daemon option is the fallback.
