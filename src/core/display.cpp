#include "core/display.h"

#include <QStringList>

#include <algorithm>
#include <cmath>

namespace contribusage {

namespace {

bool isReset(const std::optional<QDateTime> &resetsAt, const QDateTime &now) {
    return resetsAt && *resetsAt <= now;
}

} // namespace

MeterLevel meterLevel(double usedPercent) {
    if (usedPercent >= 90) {
        return MeterLevel::Critical;
    }
    return usedPercent >= 70 ? MeterLevel::Warning : MeterLevel::Normal;
}

double barFraction(double usedPercent) {
    return std::clamp(usedPercent / 100.0, 0.0, 1.0);
}

QList<double> barFractions(const QList<qint64> &values) {
    const qint64 highest = values.isEmpty() ? 0 : *std::max_element(values.cbegin(), values.cend());
    QList<double> fractions;
    fractions.reserve(values.size());
    for (const qint64 value : values) {
        fractions.append(highest > 0 ? double(value) / double(highest) : 0.0);
    }
    return fractions;
}

QString percentText(double usedPercent, bool isBelowOne, const std::optional<QDateTime> &resetsAt,
                    const QDateTime &now) {
    if (isReset(resetsAt, now)) {
        return QStringLiteral("–");
    }
    if (isBelowOne) {
        return QStringLiteral("<1%");
    }
    return QString::number(static_cast<int>(usedPercent)) + u'%';
}

std::optional<QString> resetText(const std::optional<QDateTime> &resetsAt, const QDateTime &now,
                                 const QLocale &locale) {
    if (!resetsAt) {
        return std::nullopt;
    }
    const qint64 remaining = now.secsTo(*resetsAt);
    if (remaining <= 0) {
        return QStringLiteral("reset, refreshing…");
    }
    if (remaining > 24 * 3600) {
        const QDateTime local = resetsAt->toLocalTime();
        return QStringLiteral("resets ") + locale.toString(local.date(), QStringLiteral("ddd")) + u' ' +
               locale.toString(local.time(), QLocale::ShortFormat);
    }
    const qint64 minutes = std::max<qint64>(remaining, 60) / 60; // never "0 min"
    QStringList parts;
    if (minutes >= 60) {
        parts.append(QString::number(minutes / 60) + QStringLiteral(" hr"));
    }
    if (minutes % 60 > 0) {
        parts.append(QString::number(minutes % 60) + QStringLiteral(" min"));
    }
    return QStringLiteral("resets in ") + parts.join(u' ');
}

QString ageText(const QDateTime &fetchedAt, const QDateTime &now) {
    const qint64 age = fetchedAt.secsTo(now);
    if (age < 60) {
        return QStringLiteral("updated just now");
    }
    QString unit;
    if (age >= 86400) {
        const qint64 days = age / 86400;
        unit = QString::number(days) + (days == 1 ? QStringLiteral(" day") : QStringLiteral(" days"));
    } else if (age >= 3600) {
        unit = QString::number(age / 3600) + QStringLiteral(" hr");
    } else {
        unit = QString::number(age / 60) + QStringLiteral(" min");
    }
    return QStringLiteral("updated ") + unit + QStringLiteral(" ago");
}

QString compactCount(qint64 value) {
    struct Unit {
        qint64 size;
        char16_t suffix;
    };
    static constexpr Unit units[] = {{1'000'000'000, u'B'}, {1'000'000, u'M'}, {1'000, u'K'}};
    for (const Unit &unit : units) {
        if (value < unit.size) {
            continue;
        }
        const double scaled = double(value) / double(unit.size);
        const double tenths = std::round(scaled * 10) / 10;
        if (tenths < 10) {
            QString text = QString::number(tenths, 'f', 1);
            if (text.endsWith(QStringLiteral(".0"))) {
                text.chop(2);
            }
            return text + QChar(unit.suffix);
        }
        return QString::number(std::llround(scaled)) + QChar(unit.suffix);
    }
    return QString::number(value);
}

} // namespace contribusage
