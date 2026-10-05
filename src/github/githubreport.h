#pragma once

#include <QDate>
#include <QJsonObject>
#include <QList>
#include <QString>

#include <optional>

namespace contribusage::github {

// SPEC §10.5: one day of GitHub's calendar; `level` is GitHub's contributionLevel, 0 (none) to 4 (fourth quartile).
struct ContributionDay {
    QDate date;
    int count = 0;
    int level = 0;

    bool operator==(const ContributionDay &) const = default;
};

// FR-18: the calendar as GitHub returns it; its weeks start on Sunday, the first and the last one can be short.
struct ContributionCalendar {
    QString login;
    QList<QList<ContributionDay>> weeks;
    int totalContributions = 0;

    QList<ContributionDay> days() const;
    bool operator==(const ContributionCalendar &) const = default;
};

struct ContributionStats {
    int today = 0;
    int thisWeek = 0;
    int currentStreak = 0;
    bool streakNeedsToday = false; // today is 0 so far; the streak is counted up to yesterday
    int longestStreak = 0;

    bool operator==(const ContributionStats &) const = default;
};

// FR-19, SPEC §8.4.4 over consecutive `days` in API order. Days after `today` (GitHub's day ahead of the local one)
// count nowhere; the week starts on `firstDayOfWeek` (ADR-021).
ContributionStats contributionStats(const QList<ContributionDay> &days, QDate today, Qt::DayOfWeek firstDayOfWeek);

struct GitHubReport {
    ContributionCalendar calendar;
    ContributionStats stats;

    bool operator==(const GitHubReport &) const = default;
};

// The JSON form in state.json (SPEC §10.7); nullopt for a missing or wrongly typed field.
QJsonObject toJson(const GitHubReport &report);
std::optional<GitHubReport> gitHubReportFromJson(const QJsonObject &json);

} // namespace contribusage::github
