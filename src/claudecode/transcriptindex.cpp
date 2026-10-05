#include "claudecode/transcriptindex.h"

#include <QDir>
#include <QDirIterator>
#include <QRegularExpression>

namespace contribusage::claudecode {

QStringList transcriptRoots(const QString &home, const QString &configDir) {
    QStringList roots;
    QStringList bases;
    if (configDir.startsWith(u'/')) {
        bases.append(configDir);
    }
    bases.append(home + QStringLiteral("/.claude"));
    bases.append(home + QStringLiteral("/.config/claude"));
    for (const QString &base : std::as_const(bases)) {
        const QString root = QDir::cleanPath(base + QStringLiteral("/projects"));
        if (!roots.contains(root)) {
            roots.append(root);
        }
    }
    return roots;
}

QStringList transcriptFiles(const QStringList &roots, const QString &probeFolder) {
    static const QRegularExpression outside(QStringLiteral("[^a-zA-Z0-9]"));
    QString probePath = QDir::cleanPath(probeFolder);
    const QString probeProject = probePath.replace(outside, QStringLiteral("-"));
    QStringList files;
    for (const QString &root : roots) {
        const QString skipped = root + u'/' + probeProject + u'/';
        QDirIterator it(root, {QStringLiteral("*.jsonl")}, QDir::Files, QDirIterator::Subdirectories);
        while (it.hasNext()) {
            const QString path = it.next();
            if (!path.startsWith(skipped)) {
                files.append(path);
            }
        }
    }
    return files;
}

TranscriptIndex::Entry TranscriptIndex::entry(const TranscriptLine &line) {
    return {.timestamp = line.timestamp,
            .sessionId = *m_strings.insert(line.sessionId),
            .model = *m_strings.insert(line.model),
            .tokens = line.tokens};
}

bool TranscriptIndex::update(const QStringList &files, const std::function<bool()> &interrupted) {
    QSet<QString> paths(files.cbegin(), files.cend());
    for (auto it = m_files.cbegin(); it != m_files.cend(); ++it) {
        paths.insert(it.key());
    }
    bool complete = true;
    for (const QString &path : std::as_const(paths)) {
        if (interrupted && interrupted()) {
            return false;
        }
        Transcript appended;
        const auto outcome = m_reader.read(path, [this, &appended](QByteArrayView text) {
            const auto decoded = decodeTranscriptLine(text);
            if (!decoded) {
                ++appended.skipped;
            } else if (decoded->has_value()) {
                const TranscriptLine &line = **decoded;
                const QString key = line.key();
                if (key.isEmpty()) {
                    appended.unkeyed.append(entry(line));
                } else if (!appended.keyed.contains(key)) {
                    appended.keyed.insert(key, entry(line));
                }
            }
        });
        if (!outcome) {
            complete = false; // SPEC §13: retried on the next change
            continue;
        }
        switch (*outcome) {
        case IncrementalJsonlReader::Outcome::Gone:
            m_files.remove(path);
            break;
        case IncrementalJsonlReader::Outcome::Rescanned:
            m_files.insert(path, appended);
            break;
        case IncrementalJsonlReader::Outcome::Appended: {
            Transcript &file = m_files[path];
            for (auto it = appended.keyed.cbegin(); it != appended.keyed.cend(); ++it) {
                if (!file.keyed.contains(it.key())) {
                    file.keyed.insert(it.key(), it.value());
                }
            }
            file.unkeyed.append(appended.unkeyed);
            file.skipped += appended.skipped;
            break;
        }
        }
    }
    return complete;
}

QList<ActivityDay> TranscriptIndex::days(const QTimeZone &zone) const {
    struct Bucket {
        qint64 requests = 0;
        QSet<QString> sessions;
        TokenCounts tokens;
        QMap<QString, TokenCounts> byModel;
    };
    QMap<QDate, Bucket> buckets;
    const auto add = [&buckets, &zone](const Entry &entry) {
        Bucket &bucket = buckets[entry.timestamp.toTimeZone(zone).date()];
        ++bucket.requests;
        if (!entry.sessionId.isEmpty()) {
            bucket.sessions.insert(entry.sessionId);
        }
        bucket.tokens += entry.tokens;
        bucket.byModel[entry.model] += entry.tokens;
    };
    QSet<QString> seen;
    for (auto file = m_files.cbegin(); file != m_files.cend(); ++file) {
        for (auto it = file->keyed.cbegin(); it != file->keyed.cend(); ++it) {
            if (!seen.contains(it.key())) {
                seen.insert(it.key());
                add(it.value());
            }
        }
        for (const Entry &entry : file->unkeyed) {
            add(entry);
        }
    }
    QList<ActivityDay> days;
    for (auto it = buckets.cbegin(); it != buckets.cend(); ++it) {
        days.append({.day = it.key(),
                     .requests = it->requests,
                     .sessions = it->sessions.size(),
                     .tokens = it->tokens,
                     .byModel = it->byModel});
    }
    return days;
}

qint64 TranscriptIndex::skippedLines() const {
    qint64 skipped = 0;
    for (const Transcript &file : m_files) {
        skipped += file.skipped;
    }
    return skipped;
}

} // namespace contribusage::claudecode
