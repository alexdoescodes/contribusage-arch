#include "github/githubreport.h"

#include <QJsonArray>

#include <algorithm>

namespace contribusage::github {

QList<ContributionDay> ContributionCalendar::days() const {
    QList<ContributionDay> all;
    for (const auto &week : weeks) {
        all.append(week);
    }
    return all;
}

ContributionStats contributionStats(const QList<ContributionDay> &days, QDate today, Qt::DayOfWeek firstDayOfWeek) {
    QList<ContributionDay> past;
    for (const ContributionDay &day : days) {
        if (day.date <= today) {
            past.append(day);
        }
    }
    ContributionStats stats;
    for (auto it = past.crbegin(); it != past.crend(); ++it) {
        if (it->date == today) {
            stats.today = it->count;
            break;
        }
    }
    const QDate weekStart = today.addDays(-((today.dayOfWeek() - int(firstDayOfWeek) + 7) % 7));
    for (const ContributionDay &day : past) {
        if (day.date >= weekStart) {
            stats.thisWeek += day.count;
        }
    }
    // Today at 0 so far (or not in the calendar yet) leaves the streak ending yesterday.
    auto it = past.crbegin();
    if (it != past.crend() && it->date == today && it->count == 0) {
        ++it;
    }
    for (; it != past.crend() && it->count > 0; ++it) {
        ++stats.currentStreak;
    }
    stats.streakNeedsToday = stats.today == 0 && stats.currentStreak > 0;
    int run = 0;
    for (const ContributionDay &day : past) {
        run = day.count > 0 ? run + 1 : 0;
        stats.longestStreak = std::max(stats.longestStreak, run);
    }
    return stats;
}

QJsonObject toJson(const GitHubReport &report) {
    QJsonArray weeks;
    for (const auto &week : report.calendar.weeks) {
        QJsonArray days;
        for (const ContributionDay &day : week) {
            days.append(QJsonObject{{QStringLiteral("date"), day.date.toString(Qt::ISODate)},
                                    {QStringLiteral("count"), day.count},
                                    {QStringLiteral("level"), day.level}});
        }
        weeks.append(days);
    }
    const ContributionStats &stats = report.stats;
    return {{QStringLiteral("calendar"),
             QJsonObject{{QStringLiteral("login"), report.calendar.login},
                         {QStringLiteral("weeks"), weeks},
                         {QStringLiteral("totalContributions"), report.calendar.totalContributions}}},
            {QStringLiteral("stats"), QJsonObject{{QStringLiteral("today"), stats.today},
                                                  {QStringLiteral("thisWeek"), stats.thisWeek},
                                                  {QStringLiteral("currentStreak"), stats.currentStreak},
                                                  {QStringLiteral("streakNeedsToday"), stats.streakNeedsToday},
                                                  {QStringLiteral("longestStreak"), stats.longestStreak}}}};
}

std::optional<GitHubReport> gitHubReportFromJson(const QJsonObject &json) {
    const QJsonObject calendar = json.value(QStringLiteral("calendar")).toObject();
    const QJsonObject stats = json.value(QStringLiteral("stats")).toObject();
    if (!calendar.value(QStringLiteral("login")).isString() || !calendar.value(QStringLiteral("weeks")).isArray() ||
        !calendar.value(QStringLiteral("totalContributions")).isDouble()) {
        return std::nullopt;
    }
    for (const auto *key : {"today", "thisWeek", "currentStreak", "longestStreak"}) {
        if (!stats.value(QLatin1StringView(key)).isDouble()) {
            return std::nullopt;
        }
    }
    if (!stats.value(QStringLiteral("streakNeedsToday")).isBool()) {
        return std::nullopt;
    }
    GitHubReport report{
        .calendar = {.login = calendar.value(QStringLiteral("login")).toString(),
                     .weeks = {},
                     .totalContributions = calendar.value(QStringLiteral("totalContributions")).toInt()},
        .stats = {.today = stats.value(QStringLiteral("today")).toInt(),
                  .thisWeek = stats.value(QStringLiteral("thisWeek")).toInt(),
                  .currentStreak = stats.value(QStringLiteral("currentStreak")).toInt(),
                  .streakNeedsToday = stats.value(QStringLiteral("streakNeedsToday")).toBool(),
                  .longestStreak = stats.value(QStringLiteral("longestStreak")).toInt()}};
    for (const QJsonValue &weekValue : calendar.value(QStringLiteral("weeks")).toArray()) {
        if (!weekValue.isArray()) {
            return std::nullopt;
        }
        QList<ContributionDay> week;
        for (const QJsonValue &dayValue : weekValue.toArray()) {
            const QJsonObject day = dayValue.toObject();
            const QDate date = QDate::fromString(day.value(QStringLiteral("date")).toString(), Qt::ISODate);
            if (!date.isValid() || !day.value(QStringLiteral("count")).isDouble() ||
                !day.value(QStringLiteral("level")).isDouble()) {
                return std::nullopt;
            }
            week.append({.date = date,
                         .count = day.value(QStringLiteral("count")).toInt(),
                         .level = day.value(QStringLiteral("level")).toInt()});
        }
        report.calendar.weeks.append(week);
    }
    return report;
}

} // namespace contribusage::github
