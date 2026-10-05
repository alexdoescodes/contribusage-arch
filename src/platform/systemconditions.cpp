#include "platform/systemconditions.h"

#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDBusVariant>

#include <QNetworkInformation>
#include <utility>

namespace contribusage {

ScheduleConditions afterSleepSignal(ScheduleConditions conditions, bool sleeping, const QDateTime &now) {
    conditions.asleep = sleeping;
    if (!sleeping) {
        conditions.lastWake = now;
    }
    return conditions;
}

bool lowPowerFromProfile(const QString &activeProfile) {
    return activeProfile == QStringLiteral("power-saver");
}

SystemConditions::SystemConditions(ScheduleConditionsSource &target, QObject *parent)
    : QObject(parent), m_target(target) {
    QDBusConnection::systemBus().connect(QStringLiteral("org.freedesktop.login1"),
                                         QStringLiteral("/org/freedesktop/login1"),
                                         QStringLiteral("org.freedesktop.login1.Manager"),
                                         QStringLiteral("PrepareForSleep"), this, SLOT(onPrepareForSleep(bool)));
    if (QNetworkInformation::loadBackendByFeatures(QNetworkInformation::Feature::Reachability)) {
        connect(QNetworkInformation::instance(), &QNetworkInformation::reachabilityChanged, this,
                &SystemConditions::onReachability);
        onReachability();
    }
    // power-profiles-daemon under its current name, else its old one; without either, low power stays off.
    for (const auto &[service, path] :
         {std::pair{QStringLiteral("org.freedesktop.UPower.PowerProfiles"),
                    QStringLiteral("/org/freedesktop/UPower/PowerProfiles")},
          std::pair{QStringLiteral("net.hadess.PowerProfiles"), QStringLiteral("/net/hadess/PowerProfiles")}}) {
        QDBusMessage get = QDBusMessage::createMethodCall(
            service, path, QStringLiteral("org.freedesktop.DBus.Properties"), QStringLiteral("Get"));
        get << service << QStringLiteral("ActiveProfile");
        auto *watcher = new QDBusPendingCallWatcher(QDBusConnection::systemBus().asyncCall(get), this);
        connect(watcher, &QDBusPendingCallWatcher::finished, this, [this](QDBusPendingCallWatcher *call) {
            const QDBusPendingReply<QDBusVariant> reply = *call;
            if (reply.isValid()) {
                setLowPower(lowPowerFromProfile(reply.value().variant().toString()));
            }
            call->deleteLater();
        });
        QDBusConnection::systemBus().connect(service, path, QStringLiteral("org.freedesktop.DBus.Properties"),
                                             QStringLiteral("PropertiesChanged"), this,
                                             SLOT(onPowerPropertiesChanged(QString, QVariantMap, QStringList)));
    }
}

void SystemConditions::onPrepareForSleep(bool sleeping) {
    m_target.setConditions(afterSleepSignal(m_target.conditions(), sleeping, QDateTime::currentDateTimeUtc()));
}

void SystemConditions::onPowerPropertiesChanged(const QString &, const QVariantMap &changed, const QStringList &) {
    if (changed.contains(QStringLiteral("ActiveProfile"))) {
        setLowPower(lowPowerFromProfile(changed.value(QStringLiteral("ActiveProfile")).toString()));
    }
}

void SystemConditions::setLowPower(bool lowPower) {
    ScheduleConditions conditions = m_target.conditions();
    conditions.lowPower = lowPower;
    m_target.setConditions(conditions);
}

void SystemConditions::onReachability() {
    ScheduleConditions conditions = m_target.conditions();
    // Unknown counts as online: a backend that cannot tell must not stop every source.
    conditions.online =
        QNetworkInformation::instance()->reachability() != QNetworkInformation::Reachability::Disconnected;
    m_target.setConditions(conditions);
}

} // namespace contribusage
