---
type: decision
status: accepted
updated: 2026-11-17
aliases: [ADR-052]
tracks: [src/smalltv, src/applet/popupmodel.cpp]
tags: [smalltv, hooks]
---
# ADR-052: Session states reach the SmallTV endpoint through files that Claude Code hooks write

- **Decided:** 2026-11-17 · **Spec:** FR-54, §14 Files · **Builds on:** [[decisions/0051-smalltv-endpoint]]

## Context
The clock should show when a Claude Code session waits for the user's input. The transcripts do not say: a permission prompt leaves no entry, and a finished turn looks the same as an idle window. Claude Code's documented hooks (`Stop`, `Notification`, `UserPromptSubmit`, `PostToolUse`, `SessionEnd`) do say, and SPEC §2 principle 1 allows documented hooks.

## Options
| Option | For | Against |
|---|---|---|
| Guess from the transcripts | No setup | Cannot see permission prompts; a guess |
| Hooks `POST` to the endpoint | One channel | A write path on a socket that may face the LAN, which anyone there could use |
| **Hooks write files; the endpoint reads them** | No new write path or network use; per-session state; trivial to test | The user adds hooks to `~/.claude/settings.json` |

## Decision
- A hook writes `waiting` or `running` into `<data folder>/sessions/<id>`; `SessionEnd` removes it. The endpoint reads the folder when it answers and adds `"a"` (waiting) and `"r"` (running) to the JSON; no watcher, no state of its own.
- A file older than 2 hours counts for nothing, so a session that died without `SessionEnd` cannot show "waiting" forever.
- The widget never edits `~/.claude/settings.json`; the snippet is in [[guides/smalltv-hooks]] and the user merges it (FR-39's installer is a separate matter).

## Consequences
- With no hooks, `a` and `r` are 0 and the clock shows no symbol.
- Anyone who can write into the data folder can fake a waiting session; they could do worse with that access.
