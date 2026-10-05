#include "platform/systemconditions.h"

#include <QDBusConnection>
#include <QNetworkInformation>

namespace contribusage {

ScheduleConditions afterSleepSignal(ScheduleConditions conditions, bool sleeping, const QDateTime &now) {
    conditions.asleep = sleeping;
    if (!sleeping) {
        conditions.lastWake = now;
    }
    return conditions;
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
}

void SystemConditions::onPrepareForSleep(bool sleeping) {
    m_target.setConditions(afterSleepSignal(m_target.conditions(), sleeping, QDateTime::currentDateTimeUtc()));
}

void SystemConditions::onReachability() {
    ScheduleConditions conditions = m_target.conditions();
    // Unknown counts as online: a backend that cannot tell must not stop every source.
    conditions.online =
        QNetworkInformation::instance()->reachability() != QNetworkInformation::Reachability::Disconnected;
    m_target.setConditions(conditions);
}

} // namespace contribusage
