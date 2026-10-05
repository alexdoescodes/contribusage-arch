#pragma once

#include "core/sourceschedule.h"

#include <QObject>

namespace contribusage {

// SPEC §12 rule 1: logind's PrepareForSleep(true) means asleep, (false) means awake again at `now`.
ScheduleConditions afterSleepSignal(ScheduleConditions conditions, bool sleeping, const QDateTime &now);

// Feeds the shared schedule conditions from the system: suspend and resume from logind (D-Bus, system bus) and the
// network's reachability from QNetworkInformation. Without either service the conditions stay "awake and online".
// SPEC §12 rule 3 on Linux: power-profiles-daemon's power-saver profile is Low Power Mode.
bool lowPowerFromProfile(const QString &activeProfile);

class SystemConditions : public QObject {
    Q_OBJECT

public:
    explicit SystemConditions(ScheduleConditionsSource &target, QObject *parent = nullptr);

public Q_SLOTS:
    void onPrepareForSleep(bool sleeping);
    void onPowerPropertiesChanged(const QString &interface, const QVariantMap &changed, const QStringList &invalidated);

private:
    void onReachability();
    void setLowPower(bool lowPower);

    ScheduleConditionsSource &m_target;
};

} // namespace contribusage
