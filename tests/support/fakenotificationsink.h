#pragma once

#include "core/seams.h"

// Records what would reach the desktop.
class FakeNotificationSink final : public contribusage::NotificationSink {
public:
    void notify(const contribusage::NotificationNote &note) override { notes.append(note); }

    QList<contribusage::NotificationNote> notes;
};
