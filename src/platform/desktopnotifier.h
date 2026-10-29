#pragma once

#include "core/seams.h"

#include <QHash>
#include <QObject>

namespace contribusage {

// ADR-046: notifications through the freedesktop notification service (Plasma's own), so nothing needs installing.
// A later note of the same window cycle replaces the earlier one (FR-52).
class DesktopNotifier final : public QObject, public NotificationSink {
    Q_OBJECT

public:
    using QObject::QObject;

    void notify(const NotificationNote &note) override;

private:
    QHash<QString, uint> m_serverIds; // note id → the server's id
};

} // namespace contribusage
