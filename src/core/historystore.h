#pragma once

#include "core/apppaths.h"
#include "core/jsonstore.h"
#include "core/reports.h"

#include <QDateTime>
#include <QList>
#include <QString>

#include <expected>

namespace contribusage {

// A provider's frozen daily aggregates in `providers/<id>/history.json` (SPEC §8.3.4, §10.7, FR-29). A day freezes once
// it lies before the day of `now - 48 h` and is never recomputed after that, so deleted transcripts keep their history.
class HistoryStore {
public:
    // An unreadable or other-version file is an error and stays on disk: history is never discarded.
    static std::expected<HistoryStore, StoreError> open(const QString &providerId, const AppPaths &paths);

    // Freezes the live days that are due, drops days beyond 365 and returns what to show, oldest first: frozen days,
    // then the live days not frozen. `live` holds the provider's current aggregates by local day, oldest first. A
    // failed write leaves the frozen days as they were, so the next merge tries again.
    QList<ActivityDay> merge(const QList<ActivityDay> &live, const QDateTime &now);

    // What merge() would show, without freezing anything: for a scan that could not read every file, whose counts may
    // be short.
    QList<ActivityDay> shown(const QList<ActivityDay> &live) const;

    const QList<ActivityDay> &frozen() const { return m_days; }

private:
    HistoryStore(QString providerId, QString file, QList<ActivityDay> days);

    QString m_providerId;
    QString m_file;
    QList<ActivityDay> m_days; // oldest first
};

} // namespace contribusage
