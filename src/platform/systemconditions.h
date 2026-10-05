#pragma once

#include "core/sourceschedule.h"

#include <QObject>

namespace contribusage {

// SPEC §12 rule 1: logind's PrepareForSleep(true) means asleep, (false) means awake again at `now`.
ScheduleConditions afterSleepSignal(ScheduleConditions conditions, bool sleeping, const QDateTime &now);

// Feeds the shared schedule conditions from the system: suspend and resume from logind (D-Bus, system bus) and the
// network's reachability from QNetworkInformation. Without either service the conditions stay "awake and online".
class SystemConditions : public QObject {
    Q_OBJECT

public:
    explicit SystemConditions(ScheduleConditionsSource &target, QObject *parent = nullptr);

public Q_SLOTS:
    void onPrepareForSleep(bool sleeping);

private:
    void onReachability();

    ScheduleConditionsSource &m_target;
};

} // namespace contribusage
