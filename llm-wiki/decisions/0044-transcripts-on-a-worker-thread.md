---
type: decision
status: accepted
updated: 2026-10-07
aliases: [ADR-044]
tracks: [src/claudecode/activityjob.cpp, src/claudecode/transcriptline.cpp]
tags: [performance, activity, threading]
---
# ADR-044: Transcripts are read on a worker thread

- **Decided:** 2026-10-05 · **Spec:** SPEC §8.3, FR-26, FR-27, SPEC §2 (nothing blocks the plasmashell GUI thread) · **Supersedes:** —

## Context
The widget runs inside plasmashell. Building the live index reads every transcript at start (ADR-024), which is several hundred MB on an active machine; parsing that on the GUI thread would freeze the whole desktop shell for seconds.

## Options
| Option | For | Against |
|---|---|---|
| Read on the GUI thread in slices | No threads | Still long stalls per large file; complex resumption |
| A worker `QThread` owning the index and the history store | The GUI thread only receives finished reports; the index stays single-threaded | A thread per widget instance; shutdown must stop a scan |
| A separate helper process | Isolation | Another binary, IPC, packaging |

## Decision
`ActivityJob` owns a low-priority worker thread. The `ActivityWorker` there owns the `TranscriptIndex` and the `HistoryStore` and is the only code touching them; scans are requested by a queued call and their reports come back by a queued call. Destruction asks the thread to stop between two files and waits for it. Lines without the text `"usage"` are not parsed, so a cold scan stays short; such lines are not counted as skipped even when malformed.

## Consequences
- The `FileReading` seam of SPEC §10.6 is not built yet: tests use real temporary files, and the declared-roots conformance check (§16.4) arrives with a second activity provider.
- The skipped-lines diagnostic counts only lines that mention `"usage"`.
