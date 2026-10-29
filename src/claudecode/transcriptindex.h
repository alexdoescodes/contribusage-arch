#pragma once

#include "claudecode/transcriptline.h"
#include "core/jsonlreader.h"
#include "core/reports.h"

#include <QHash>
#include <QList>
#include <QMap>
#include <QSet>
#include <QStringList>
#include <QTimeZone>

#include <functional>

namespace contribusage::claudecode {

// FR-22 roots in order, without duplicates, whether they exist or not: `configDir/projects` when `configDir` (the login
// shell's CLAUDE_CONFIG_DIR) is an absolute path, then ~/.claude/projects and ~/.config/claude/projects.
QStringList transcriptRoots(const QString &home, const QString &configDir);

// `**/*.jsonl` under `roots`, without the project folder Claude Code would make for `probeFolder` (FR-28): every
// character outside [a-zA-Z0-9] of the working directory becomes `-` (SPEC §8.3.1).
QStringList transcriptFiles(const QStringList &roots, const QString &probeFolder);

// The live index (SPEC §8.3.4, ADR-024): per transcript its usage lines, one per key, and how many lines it could not
// decode. The days are rebuilt from all files, because a key repeats across files (resumed sessions) and a file can
// shrink or vanish.
class TranscriptIndex {
public:
    // Reads `files` and every file indexed before: new lines appended, rescanned files from scratch, vanished files
    // dropped. An unreadable file keeps what it had and is retried next time (SPEC §13). `interrupted` is asked before
    // each file; when it returns true the update stops there. Returns whether every file was read.
    bool update(const QStringList &files, const std::function<bool()> &interrupted = {});

    // Per local day of `zone`, oldest first; a key counts once across files, the first in path order (FR-24, FR-25).
    QList<ActivityDay> days(const QTimeZone &zone) const;

    qint64 skippedLines() const;

private:
    // A usage line without the IDs its key holds; strings are shared through `m_strings` (NFR-2).
    struct Entry {
        QDateTime timestamp;
        QString sessionId;
        QString model;
        TokenCounts tokens;
    };
    struct Transcript {
        QHash<QString, Entry> keyed;
        QList<Entry> unkeyed;
        qint64 skipped = 0;
    };

    Entry entry(const TranscriptLine &line);

    IncrementalJsonlReader m_reader;
    QMap<QString, Transcript> m_files; // path order
    QSet<QString> m_strings;
};

} // namespace contribusage::claudecode
