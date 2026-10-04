#pragma once

#include <QDateTime>
#include <QList>
#include <QLocale>
#include <QString>

#include <optional>

namespace contribusage {

// SPEC §11.4: the meter color's level for a used percentage.
enum class MeterLevel { Normal, Warning, Critical };
MeterLevel meterLevel(double usedPercent);

// How much of a meter is filled, 0 to 1: usage can exceed 100 %, a bar can't.
double barFraction(double usedPercent);

// FR-11: past its reset a window's value is unknown, so its bar is empty and its level normal.
double barFraction(double usedPercent, const std::optional<QDateTime> &resetsAt, const QDateTime &now);
MeterLevel meterLevel(double usedPercent, const std::optional<QDateTime> &resetsAt, const QDateTime &now);

// Bar heights relative to the largest value; all 0 when nothing happened.
QList<double> barFractions(const QList<qint64> &values);

// SPEC §11.5, FR-11: "23%", "<1%", or "–" once the window has reset.
QString percentText(double usedPercent, bool isBelowOne, const std::optional<QDateTime> &resetsAt,
                    const QDateTime &now);

// SPEC §11.5: "resets in 2 hr 10 min", "resets Thu 1:58 PM", "reset, refreshing…"; nullopt without a reset time.
std::optional<QString> resetText(const std::optional<QDateTime> &resetsAt, const QDateTime &now,
                                 const QLocale &locale = QLocale());

// SPEC §11.5: "updated just now", "updated 2 min ago", "updated 3 hr ago", "updated 2 days ago".
QString ageText(const QDateTime &fetchedAt, const QDateTime &now);

// SPEC §11.5: compact counts, "668", "1.2K", "812K", "4.2M".
QString compactCount(qint64 value);

// ADR-042: the popup shows one week bar. "Current week (all models)" reads "Current week"; a model's own week window,
// "Current week (Opus)", is hidden (nullopt). Any other window keeps its printed label (FR-8).
std::optional<QString> shownWindowLabel(const QString &printed);

} // namespace contribusage
