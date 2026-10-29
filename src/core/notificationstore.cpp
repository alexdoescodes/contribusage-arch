#include "core/notificationstore.h"

#include "core/jsonstore.h"
#include "core/limitscache.h"

#include <QJsonObject>

namespace contribusage::notificationstore {

namespace {

QString file(const AppPaths &paths) {
    return paths.root + QStringLiteral("/state.json");
}

QJsonObject state(const AppPaths &paths) {
    const auto read = jsonstore::read(file(paths), limitscache::schemaVersion);
    return read && read->has_value() ? read->value().toObject() : QJsonObject();
}

} // namespace

NotificationsSent load(const AppPaths &paths) {
    NotificationsSent sent;
    const QJsonObject providers = state(paths).value(QStringLiteral("notifications")).toObject();
    for (auto provider = providers.begin(); provider != providers.end(); ++provider) {
        const QJsonObject windows = provider.value().toObject();
        for (auto window = windows.begin(); window != windows.end(); ++window) {
            const QJsonObject cycle = window.value().toObject();
            const QDateTime sentAt =
                QDateTime::fromString(cycle.value(QStringLiteral("sentAt")).toString(), Qt::ISODateWithMs);
            const int threshold = cycle.value(QStringLiteral("threshold")).toInt();
            if (!sentAt.isValid() || threshold <= 0) {
                continue;
            }
            std::optional<QDateTime> resetsAt;
            if (const QString text = cycle.value(QStringLiteral("resetsAt")).toString(); !text.isEmpty()) {
                resetsAt = QDateTime::fromString(text, Qt::ISODateWithMs);
            }
            sent[provider.key()][window.key()] = {.resetsAt = resetsAt, .threshold = threshold, .sentAt = sentAt};
        }
    }
    return sent;
}

bool save(const AppPaths &paths, const NotificationsSent &sent) {
    QJsonObject providers;
    for (auto provider = sent.cbegin(); provider != sent.cend(); ++provider) {
        QJsonObject windows;
        for (auto window = provider->cbegin(); window != provider->cend(); ++window) {
            QJsonObject cycle{{QStringLiteral("threshold"), window->threshold},
                              {QStringLiteral("sentAt"), window->sentAt.toUTC().toString(Qt::ISODateWithMs)}};
            if (window->resetsAt) {
                cycle.insert(QStringLiteral("resetsAt"), window->resetsAt->toUTC().toString(Qt::ISODateWithMs));
            }
            windows.insert(window.key(), cycle);
        }
        providers.insert(provider.key(), windows);
    }
    QJsonObject root = state(paths);
    root.insert(QStringLiteral("notifications"), providers);
    return jsonstore::write(file(paths), limitscache::schemaVersion, root).has_value();
}

} // namespace contribusage::notificationstore
