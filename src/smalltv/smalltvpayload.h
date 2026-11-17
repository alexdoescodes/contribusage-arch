#pragma once

#include "core/snapshot.h"

#include <QByteArray>
#include <QDateTime>
#include <QString>

#include <optional>

namespace contribusage::smalltv {

// ADR-051: how long a probe result stays good to serve; older data makes the server answer 503.
inline constexpr qint64 staleAfterSeconds = 15 * 60;

// ADR-052: how long a session's state file is believed. A session that died without its SessionEnd hook, or a
// forgotten window, stops counting after this.
inline constexpr qint64 sessionExpirySeconds = 2 * 60 * 60;

// The Claude Code sessions the hooks have reported: waiting for the user's input, or working.
struct SessionCounts {
    int waiting = 0;
    int running = 0;
};

// Reads `<folder>/<session id>`: one small file per session holding `waiting` or `running`, written by the user's
// Claude Code hooks (ADR-052). Files older than `sessionExpirySeconds`, other contents, folders and a missing folder
// count for nothing. An empty `folder` gives zeros.
SessionCounts readSessions(const QString &folder, const QDateTime &now);

// A reset countdown as the 240x240 SmallTV text shows it: "45m", "1h 35m", "2d 23h". The theme cannot do arithmetic,
// so the widget does the splitting. Negative input reads as "0m".
QString formatReset(qint64 minutes);

// The JSON body of GET /: {"s","sr","srt","w","wr","wrt","a","r","ok":true}; `a` is the number of sessions waiting for
// the user, `r` the number working (ADR-052). `s`/`w` are the session and week percentages
// rounded, `sr`/`wr` the minutes to the reset, `srt`/`wrt` the same split by `formatReset`. A window the report lacks
// leaves its three keys out. nullopt when there is no report, it is older than `staleAfterSeconds`, or it carries
// neither window; the caller then answers 503 so the clock face takes over.
std::optional<QByteArray> payload(const std::optional<Snapshot<LimitsReport>> &snapshot, const QDateTime &now,
                                  SessionCounts sessions = {});

} // namespace contribusage::smalltv
