---
type: module
status: active
updated: 2026-11-17
tracks: [src/smalltv, tests/smalltv]
tags: [smalltv, network]
---
# ContribusageSmallTV

`src/smalltv/`, target `contribusage_smalltv` (links the core and `Qt6::Network`): the opt-in endpoint of FR-53, decided in [[decisions/0051-smalltv-endpoint]].

- `smalltvpayload` is pure: `formatReset(minutes)` gives `45m`, `1h 35m` or `2d 23h`, and `payload(snapshot, now)` builds the JSON body from the last limits snapshot, or nothing when there is none, it is older than `staleAfterSeconds` (15 min) or it has neither a session nor a weekly window.
- `smalltvserver` is a `QTcpServer` that answers `GET /` with `200` and the payload or `503`, `404` for another path and `405` for another method. It takes the snapshot with `setSnapshot` and the time from an injectable clock, so tests need no real time.
- [[modules/applet]]'s `PopupModel` owns the server, feeds it after every limits job change, and opens, moves or closes it from the `smalltvEnabled`, `smalltvAddress` and `smalltvPort` settings; `Footer.qml` has the switch and `ConfigSmallTV.qml` the page.

`tests/smalltv/tst_smalltv.cpp` covers the countdown formats, the payload keys, a missing window, stale and missing data, and the server's status codes over a real loopback socket.

`readSessions(folder, now)` counts the session files the hooks write ([[decisions/0052-session-states-from-hooks]], [[guides/smalltv-hooks]]); the server calls it for each answer and `payload` adds `a` and `r`. Files older than 2 hours, other contents and folders count for nothing.
