#include "core/historystore.h"

#include <QJsonObject>
#include <QSet>

#include <algorithm>

namespace contribusage {

namespace {
constexpr int schemaVersion = 1;
}

HistoryStore::HistoryStore(QString providerId, QString file, QList<ActivityDay> days)
    : m_providerId(std::move(providerId)), m_file(std::move(file)), m_days(std::move(days)) {}

std::expected<HistoryStore, StoreError> HistoryStore::open(const QString &providerId, const AppPaths &paths) {
    const QString file = paths.providerFolder(providerId) + QStringLiteral("/history.json");
    const auto read = jsonstore::read(file, schemaVersion);
    if (!read) {
        return std::unexpected(read.error());
    }
    if (!read->has_value()) {
        return HistoryStore(providerId, file, {});
    }
    const auto report = activityReportFromJson(read->value().toObject());
    if (!report) {
        return std::unexpected(StoreError{.kind = StoreError::Kind::Malformed, .version = 0, .message = file});
    }
    return HistoryStore(providerId, file, report->days);
}

QList<ActivityDay> HistoryStore::merge(const QList<ActivityDay> &live, const QDateTime &now) {
    const QDate firstLive = now.addSecs(-48 * 3600).date();
    const QDate oldestKept = now.date().addDays(-364);
    QSet<QDate> frozen;
    for (const ActivityDay &day : m_days) {
        frozen.insert(day.day);
    }
    QList<ActivityDay> unfrozen;
    for (const ActivityDay &day : live) {
        if (!frozen.contains(day.day)) {
            unfrozen.append(day);
        }
    }
    QList<ActivityDay> updated;
    for (const ActivityDay &day : m_days) {
        if (day.day >= oldestKept) {
            updated.append(day);
        }
    }
    for (const ActivityDay &day : unfrozen) {
        if (day.day < firstLive && day.day >= oldestKept) {
            updated.append(day);
        }
    }
    std::ranges::sort(updated, {}, &ActivityDay::day);
    if (updated != m_days) {
        const ActivityReport report{.provider = m_providerId, .days = updated, .skippedLines = 0};
        if (jsonstore::write(m_file, schemaVersion, toJson(report))) {
            m_days = updated;
        }
    }
    QList<ActivityDay> shown = updated;
    for (const ActivityDay &day : unfrozen) {
        if (day.day >= firstLive) {
            shown.append(day);
        }
    }
    return shown;
}

QList<ActivityDay> HistoryStore::shown(const QList<ActivityDay> &live) const {
    QSet<QDate> frozen;
    for (const ActivityDay &day : m_days) {
        frozen.insert(day.day);
    }
    QList<ActivityDay> shown = m_days;
    for (const ActivityDay &day : live) {
        if (!frozen.contains(day.day)) {
            shown.append(day);
        }
    }
    std::ranges::sort(shown, {}, &ActivityDay::day);
    return shown;
}

} // namespace contribusage
