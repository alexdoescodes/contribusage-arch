#pragma once

#include <QDate>
#include <QDateTime>
#include <QJsonObject>
#include <QList>
#include <QMap>
#include <QString>

#include <optional>

namespace contribusage {

// SPEC §10.1: why a source has no current value. Which fields carry data depends on the kind.
struct SourceError {
    enum class Kind {
        ToolNotFound,
        NotLoggedIn,
        UnsupportedPlan, // text: the tool's note (ADR-017)
        TimedOut,
        ProcessFailed, // code: exit code, text: stderr tail
        Unparseable,   // text: raw output
        Offline,
        TokenMissing,
        Unauthorized,
        RateLimited, // until
        Http,        // code: status
        Decoding,    // text: message
        Io,          // text: message
        ProviderSpecific,
    };
    Kind kind;
    QString text = {};
    int code = 0;
    QDateTime until = {};

    bool operator==(const SourceError &) const = default;
};

enum class WindowKind { Session, Weekly, Other };

// SPEC §10.3: one plan limit bucket, its label verbatim (FR-8).
struct UsageWindow {
    QString label;
    WindowKind kind = WindowKind::Other;
    double usedPercent = 0; // can exceed 100; only the UI clamps
    bool isBelowOne = false;
    std::optional<QDateTime> resetsAt;

    bool operator==(const UsageWindow &) const = default;
};

// FR-38: `percent` is empty for a line the provider could not read; `label` then holds the whole line.
struct Share {
    QString label;
    std::optional<int> percent;

    bool operator==(const Share &) const = default;
};

struct Ranking {
    QString title;
    QList<Share> items;

    bool operator==(const Ranking &) const = default;
};

struct InsightsPeriod {
    QString label;
    QString summary;
    QList<Share> shares;
    QList<Ranking> rankings;

    bool operator==(const InsightsPeriod &) const = default;
};

// FR-38, ADR-023: tool supplied, approximate statistics; at least one period.
struct Insights {
    std::optional<QString> note;
    QList<InsightsPeriod> periods;

    bool operator==(const Insights &) const = default;
};

struct LimitsReport {
    QString provider;
    QList<UsageWindow> windows;
    std::optional<QString> billingNote;
    std::optional<Insights> insights;
    std::optional<QString> rawOutput;

    bool operator==(const LimitsReport &) const = default;
};

struct TokenCounts {
    qint64 input = 0;
    qint64 output = 0;
    qint64 cacheWrite = 0;
    qint64 cacheRead = 0;

    qint64 total() const { return input + output + cacheWrite + cacheRead; }
    TokenCounts &operator+=(const TokenCounts &other);
    bool operator==(const TokenCounts &) const = default;
};

// SPEC §10.4: one local calendar day.
struct ActivityDay {
    QDate day;
    qint64 requests = 0;
    qint64 sessions = 0;
    TokenCounts tokens;
    QMap<QString, TokenCounts> byModel;

    bool operator==(const ActivityDay &) const = default;
};

struct ActivityReport {
    QString provider;
    QList<ActivityDay> days; // oldest first, up to 365
    qint64 skippedLines = 0;

    bool operator==(const ActivityReport &) const = default;
};

// The JSON form inside `state.json` and `history.json` (SPEC §10.7). `fromJson` returns nullopt when a required field
// is missing or has the wrong type, so a foreign or damaged file never yields a half-filled report.
QJsonObject toJson(const LimitsReport &report);
std::optional<LimitsReport> limitsReportFromJson(const QJsonObject &json);
QJsonObject toJson(const ActivityReport &report);
std::optional<ActivityReport> activityReportFromJson(const QJsonObject &json);

} // namespace contribusage
