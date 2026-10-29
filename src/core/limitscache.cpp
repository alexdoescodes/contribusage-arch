#include "core/limitscache.h"

#include "core/jsonstore.h"

#include <QJsonObject>

namespace contribusage::limitscache {

namespace {

QString file(const AppPaths &paths) {
    return paths.root + QStringLiteral("/state.json");
}

QJsonObject state(const AppPaths &paths) {
    const auto read = jsonstore::read(file(paths), schemaVersion);
    return read && read->has_value() ? read->value().toObject() : QJsonObject();
}

} // namespace

std::optional<Snapshot<LimitsReport>> load(const AppPaths &paths, const QString &providerId) {
    const QJsonObject entry = state(paths).value(QStringLiteral("limits")).toObject().value(providerId).toObject();
    const QDateTime fetchedAt =
        QDateTime::fromString(entry.value(QStringLiteral("fetchedAt")).toString(), Qt::ISODateWithMs);
    const auto report = limitsReportFromJson(entry.value(QStringLiteral("report")).toObject());
    if (!fetchedAt.isValid() || !report) {
        return std::nullopt;
    }
    return Snapshot<LimitsReport>{
        .value = *report, .fetchedAt = fetchedAt, .origin = entry.value(QStringLiteral("origin")).toString()};
}

bool save(const AppPaths &paths, const Snapshot<LimitsReport> &snapshot) {
    QJsonObject root = state(paths);
    QJsonObject limits = root.value(QStringLiteral("limits")).toObject();
    limits.insert(snapshot.value.provider,
                  QJsonObject{{QStringLiteral("fetchedAt"), snapshot.fetchedAt.toUTC().toString(Qt::ISODateWithMs)},
                              {QStringLiteral("origin"), snapshot.origin},
                              {QStringLiteral("report"), toJson(snapshot.value)}});
    root.insert(QStringLiteral("limits"), limits);
    return jsonstore::write(file(paths), schemaVersion, root).has_value();
}

bool remove(const AppPaths &paths, const QString &providerId) {
    QJsonObject root = state(paths);
    QJsonObject limits = root.value(QStringLiteral("limits")).toObject();
    limits.remove(providerId);
    root.insert(QStringLiteral("limits"), limits);
    return jsonstore::write(file(paths), schemaVersion, root).has_value();
}

} // namespace contribusage::limitscache
