---
type: decision
status: accepted
updated: 2026-10-29
aliases: [ADR-043]
tracks: [src/applet/GitHubSection.qml]
tags: [ui, github, security]
---
# ADR-043: The GitHub token is entered in the popup

- **Decided:** 2026-10-05 · **Spec:** FR-16, FR-17, SPEC §11.3 · **Supersedes:** —

## Context
SPEC §11.3 had the not-connected GitHub block open Settings, where the token is entered. The Plasma widget has no Settings pages yet (L-5), and GitHub was wanted before them.

## Options
| Option | For | Against |
|---|---|---|
| Wait for the Settings pages | One place for all settings | GitHub stays hidden until L-5 |
| Token form in the GitHub block | Works now; the place the user looks when GitHub is missing | A password field in a popup |
| Import from `gh auth token` (FR-21) | No typing | Reads another tool's credential store; FR-21 is P3 and button-only |

## Decision
While no token is saved, and after GitHub rejected the saved one (401), the GitHub block shows a password field, Connect and a link to create a read-only token. The token is validated with `viewer { login }`, then saved in KWallet; the field is cleared on submit and the token never becomes a QML property.

## Consequences
- The Settings pages (L-5) add Replace and Disconnect there; the popup form stays as the not-configured state.
- A disconnect button sits in the GitHub header meanwhile.
