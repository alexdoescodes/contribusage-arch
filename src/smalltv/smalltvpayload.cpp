#include "smalltv/smalltvpayload.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>

#include <algorithm>
#include <cmath>

namespace contribusage::smalltv {

QString formatReset(qint64 minutes) {
    if (minutes < 1) {
        return QStringLiteral("0m");
    }
    if (minutes < 60) {
        return QStringLiteral("%1m").arg(minutes);
    }
    const qint64 hours = minutes / 60;
    if (minutes < 24 * 60) {
        const qint64 rest = minutes % 60;
        return rest == 0 ? QStringLiteral("%1h").arg(hours) : QStringLiteral("%1h %2m").arg(hours).arg(rest);
    }
    const qint64 days = hours / 24;
    const qint64 restHours = hours % 24;
    return restHours == 0 ? QStringLiteral("%1d").arg(days) : QStringLiteral("%1d %2h").arg(days).arg(restHours);
}

SessionCounts readSessions(const QString &folder, const QDateTime &now) {
    SessionCounts counts;
    if (folder.isEmpty()) {
        return counts;
    }
    const QFileInfoList entries = QDir(folder).entryInfoList(QDir::Files | QDir::NoDotAndDotDot);
    for (const QFileInfo &entry : entries) {
        const qint64 age = entry.lastModified().secsTo(now);
        if (age > sessionExpirySeconds || entry.size() > 32) {
            continue;
        }
        QFile file(entry.filePath());
        if (!file.open(QIODevice::ReadOnly)) {
            continue;
        }
        const QByteArray state = file.readAll().trimmed();
        if (state == "waiting") {
            ++counts.waiting;
        } else if (state == "running") {
            ++counts.running;
        }
    }
    return counts;
}

namespace {

void addWindow(QJsonObject &json, const UsageWindow &window, const QDateTime &now, const char *percent,
               const char *minutes, const char *text) {
    json.insert(QLatin1StringView(percent), int(std::lround(window.usedPercent)));
    const qint64 left = window.resetsAt ? std::max<qint64>(0, now.secsTo(*window.resetsAt) / 60) : 0;
    json.insert(QLatin1StringView(minutes), left);
    json.insert(QLatin1StringView(text), formatReset(left));
}

} // namespace

std::optional<QByteArray> payload(const std::optional<Snapshot<LimitsReport>> &snapshot, const QDateTime &now,
                                  SessionCounts sessions) {
    if (!snapshot || snapshot->isStale(now, staleAfterSeconds)) {
        return std::nullopt;
    }
    const UsageWindow *session = nullptr;
    const UsageWindow *weekly = nullptr;
    for (const UsageWindow &window : snapshot->value.windows) {
        if (window.kind == WindowKind::Session && !session) {
            session = &window;
        } else if (window.kind == WindowKind::Weekly && !weekly) {
            weekly = &window;
        }
    }
    if (!session && !weekly) {
        return std::nullopt;
    }
    QJsonObject json;
    if (session) {
        addWindow(json, *session, now, "s", "sr", "srt");
    }
    if (weekly) {
        addWindow(json, *weekly, now, "w", "wr", "wrt");
    }
    json.insert(QStringLiteral("a"), sessions.waiting);
    json.insert(QStringLiteral("r"), sessions.running);
    json.insert(QStringLiteral("ok"), true);
    return QJsonDocument(json).toJson(QJsonDocument::Compact);
}

} // namespace contribusage::smalltv
