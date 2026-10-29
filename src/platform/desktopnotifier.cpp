#include "platform/desktopnotifier.h"

#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QPointer>
#include <QVariantMap>

namespace contribusage {

void DesktopNotifier::notify(const NotificationNote &note) {
    QDBusMessage message = QDBusMessage::createMethodCall(
        QStringLiteral("org.freedesktop.Notifications"), QStringLiteral("/org/freedesktop/Notifications"),
        QStringLiteral("org.freedesktop.Notifications"), QStringLiteral("Notify"));
    message << QStringLiteral("contribusage") << m_serverIds.value(note.id, 0u)
            << QStringLiteral("utilities-system-monitor") << note.title << QString() << QStringList()
            << QVariantMap{{QStringLiteral("x-kde-origin-name"), note.provider}} << -1;
    auto *watcher = new QDBusPendingCallWatcher(QDBusConnection::sessionBus().asyncCall(message), this);
    const QString id = note.id;
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this, id](QDBusPendingCallWatcher *call) {
        const QDBusPendingReply<uint> reply = *call;
        if (reply.isValid()) {
            m_serverIds.insert(id, reply.value());
        }
        call->deleteLater();
    });
}

} // namespace contribusage
