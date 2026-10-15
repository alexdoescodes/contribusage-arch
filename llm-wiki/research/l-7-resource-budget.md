---
type: research
status: answered
updated: 2026-10-15
tracks: [src/applet/popupmodel.cpp, src/claudecode/activityjob.cpp, packaging/arch/PKGBUILD]
tags: [research, performance, packaging]
---
# L-7: the resource budget measured

- **Spec:** SPEC §17.8 L-7, port design §4 (resource budget) · **Answered:** 2026-10-15

## Question
Does the packaged plugin stay inside the port design's three checks: under 15 MB added memory, idle CPU about 0, a refresh pass under 50 ms?

## Method
- Memory: `plasmoidviewer -c org.kde.panel -a <applet> -f horizontal -l topedge -s 500x220` with the Release plugin, 60 s after start, Private_Dirty summed from `/proc/<pid>/smaps_rollup`. The baseline is KDE's compiled `org.kde.plasma.analogclock`; the number is the widget run minus the baseline run. Private_Dirty and not RSS, because RSS also counts shared library pages the shell already maps.
- Idle CPU: the same panel viewer with the popup closed, `utime+stime` from 60 s to 660 s.
- Refresh pass: a scratch program linking the built core and Claude Code libraries, timing the transcript index and the views on a real transcript folder (300 files, 471 MB).
- Machine: a laptop with 13 GB RAM running Plasma 6 on Wayland. Runs are noisy by about 4 MB, so each figure is a median of several runs.

## Findings
| Check | Budget | Result |
|---|---|---|
| Added memory, before the arena cap | < 15 MB | 17.8, 18.4, 18.5 MB (RSS delta 24-26 MB) |
| Added memory, `mallopt(M_ARENA_MAX, 1)` in the constructor | < 15 MB | 13.3, 18.2, 15.8 MB (median 15.8) |
| Added memory, the same call from a static initializer | < 15 MB | 18.6, 16.0, 9.7 MB (median 16.0) |
| Added memory, `MALLOC_ARENA_MAX=1` in the viewer's environment | < 15 MB | 13.5, 8.1 MB (two runs) |
| Idle CPU, popup closed, 600 s | about 0 | 0.09 % (an upper bound: it includes the viewer and rescans while this project's own session wrote transcripts) |
| Cold scan, 300 files / 471 MB | NFR-7 | 0.93-1.25 s CPU on the worker |
| Incremental scan after an append | NFR-7, < 200 ms | 8-16 ms |
| View work of a refresh | < 50 ms | 0.39-0.55 ms |

Where the memory goes: the UI adds 7.7 MB (measured with fake data), live data another 10.8 MB: the transcript index (4.4-5.6 MB), the worker thread and the threads Qt and D-Bus start, with their malloc arenas.

Tried and dropped:
- `malloc_trim(0)` after each scan and 64-bit hashed index keys: the index itself shrank from 4.3 to 2.5 MB in the bench, but the whole process did not improve in alternating 120 s runs (median 78.8 MB without, 82.3 MB with).
- Capping the arenas from inside the plugin recovers only part of what the environment variable does, because threads that exist before the plugin loads keep their arenas. The call stays, since it costs one line and the three runs with it are no worse; the gain is within the noise.

## Conclusion
CPU and refresh cost are far inside their checks. Memory sits at 16 MB median against a 15 MB check, 1 MB over and inside the measurement noise. [[decisions/0048-plasmoid-memory-budget]] sets the budget to 20 MB. Reaching 15 MB reliably would need fewer threads or a smaller index, which the measurements do not justify.
