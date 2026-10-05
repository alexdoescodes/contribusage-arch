#pragma once

#include "core/reports.h"

#include <QByteArrayView>
#include <QDateTime>
#include <QString>

#include <expected>
#include <optional>

namespace contribusage::claudecode {

// One usage-bearing line of a Claude Code transcript (SPEC §8.3.2, FR-23).
struct TranscriptLine {
    QDateTime timestamp;
    QString sessionId;
    QString requestId;
    QString messageId;
    QString model;
    bool isSidechain = false;
    TokenCounts tokens;

    // FR-24: `message.id:requestId`, `message.id` alone, or empty for a line without either.
    QString key() const;
};

// nullopt: a line that carries no usage (not `assistant`, no `message.usage`, a placeholder model such as
// `<synthetic>`). An error: not JSON, or no usable `timestamp`; the caller counts it as skipped (FR-23). A line without
// the text `"usage"` cannot carry usage and is not parsed at all, which keeps a scan of ~0.5 GB of transcripts short.
std::expected<std::optional<TranscriptLine>, QString> decodeTranscriptLine(QByteArrayView line);

} // namespace contribusage::claudecode
