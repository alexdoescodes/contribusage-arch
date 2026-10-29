#pragma once

#include "core/reports.h"

#include <QDateTime>
#include <QString>
#include <QTimeZone>

namespace contribusage::claudecode {

inline QString providerId() {
    return QStringLiteral("claude-code");
}

// SPEC §8.1.4: an unknown label is kept as Other, never dropped.
WindowKind windowKind(const QString &label);

// SPEC §8.1.3 P-1 to P-11: the report from `claude -p /usage` output. Whether the billing note means a subscription is
// the caller's decision (P-10). `fallbackZone` applies when a reset clause names no known zone (P-6).
LimitsReport parseUsage(const QString &output, const QDateTime &now,
                        const QTimeZone &fallbackZone = QTimeZone::systemTimeZone());

} // namespace contribusage::claudecode
