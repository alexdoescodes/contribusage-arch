#include "core/reports.h"

#include <QJsonArray>

namespace contribusage {

TokenCounts &TokenCounts::operator+=(const TokenCounts &other) {
    input += other.input;
    output += other.output;
    cacheWrite += other.cacheWrite;
    cacheRead += other.cacheRead;
    return *this;
}

namespace {

QJsonValue optionalString(const std::optional<QString> &text) {
    return text ? QJsonValue(*text) : QJsonValue();
}

std::optional<QString> stringOrNull(const QJsonValue &value) {
    return value.isString() ? std::optional<QString>(value.toString()) : std::nullopt;
}

QString kindName(WindowKind kind) {
    switch (kind) {
    case WindowKind::Session:
        return QStringLiteral("session");
    case WindowKind::Weekly:
        return QStringLiteral("weekly");
    case WindowKind::Other:
        break;
    }
    return QStringLiteral("other");
}

std::optional<WindowKind> kindFromName(const QString &name) {
    if (name == u"session") {
        return WindowKind::Session;
    }
    if (name == u"weekly") {
        return WindowKind::Weekly;
    }
    if (name == u"other") {
        return WindowKind::Other;
    }
    return std::nullopt;
}

QJsonValue dateTimeJson(const std::optional<QDateTime> &time) {
    return time ? QJsonValue(time->toUTC().toString(Qt::ISODateWithMs)) : QJsonValue();
}

QJsonArray sharesJson(const QList<Share> &shares) {
    QJsonArray array;
    for (const Share &share : shares) {
        array.append(
            QJsonObject{{QStringLiteral("label"), share.label},
                        {QStringLiteral("percent"), share.percent ? QJsonValue(*share.percent) : QJsonValue()}});
    }
    return array;
}

std::optional<QList<Share>> sharesFrom(const QJsonValue &value) {
    if (!value.isArray()) {
        return std::nullopt;
    }
    QList<Share> shares;
    for (const QJsonValue &item : value.toArray()) {
        const QJsonObject object = item.toObject();
        if (!object.value(QStringLiteral("label")).isString()) {
            return std::nullopt;
        }
        const QJsonValue percent = object.value(QStringLiteral("percent"));
        shares.append(Share{.label = object.value(QStringLiteral("label")).toString(),
                            .percent = percent.isDouble() ? std::optional<int>(percent.toInt()) : std::nullopt});
    }
    return shares;
}

QJsonObject tokensJson(const TokenCounts &tokens) {
    return {{QStringLiteral("input"), tokens.input},
            {QStringLiteral("output"), tokens.output},
            {QStringLiteral("cacheWrite"), tokens.cacheWrite},
            {QStringLiteral("cacheRead"), tokens.cacheRead}};
}

// The four counts, each required to be a number.
std::optional<TokenCounts> tokensFrom(const QJsonValue &value) {
    const QJsonObject object = value.toObject();
    for (const auto *key : {"input", "output", "cacheWrite", "cacheRead"}) {
        if (!object.value(QLatin1StringView(key)).isDouble()) {
            return std::nullopt;
        }
    }
    return TokenCounts{.input = object.value(QStringLiteral("input")).toInteger(),
                       .output = object.value(QStringLiteral("output")).toInteger(),
                       .cacheWrite = object.value(QStringLiteral("cacheWrite")).toInteger(),
                       .cacheRead = object.value(QStringLiteral("cacheRead")).toInteger()};
}

QJsonObject insightsJson(const Insights &insights) {
    QJsonArray periods;
    for (const InsightsPeriod &period : insights.periods) {
        QJsonArray rankings;
        for (const Ranking &ranking : period.rankings) {
            rankings.append(QJsonObject{{QStringLiteral("title"), ranking.title},
                                        {QStringLiteral("items"), sharesJson(ranking.items)}});
        }
        periods.append(QJsonObject{{QStringLiteral("label"), period.label},
                                   {QStringLiteral("summary"), period.summary},
                                   {QStringLiteral("shares"), sharesJson(period.shares)},
                                   {QStringLiteral("rankings"), rankings}});
    }
    return {{QStringLiteral("note"), optionalString(insights.note)}, {QStringLiteral("periods"), periods}};
}

std::optional<Insights> insightsFrom(const QJsonObject &object) {
    // FR-38: at least one period.
    if (!object.value(QStringLiteral("periods")).isArray() ||
        object.value(QStringLiteral("periods")).toArray().isEmpty()) {
        return std::nullopt;
    }
    Insights insights{.note = stringOrNull(object.value(QStringLiteral("note"))), .periods = {}};
    for (const QJsonValue &item : object.value(QStringLiteral("periods")).toArray()) {
        const QJsonObject period = item.toObject();
        const auto shares = sharesFrom(period.value(QStringLiteral("shares")));
        if (!period.value(QStringLiteral("label")).isString() || !period.value(QStringLiteral("summary")).isString() ||
            !shares || !period.value(QStringLiteral("rankings")).isArray()) {
            return std::nullopt;
        }
        QList<Ranking> rankings;
        for (const QJsonValue &rankingItem : period.value(QStringLiteral("rankings")).toArray()) {
            const QJsonObject ranking = rankingItem.toObject();
            const auto items = sharesFrom(ranking.value(QStringLiteral("items")));
            if (!ranking.value(QStringLiteral("title")).isString() || !items) {
                return std::nullopt;
            }
            rankings.append(Ranking{.title = ranking.value(QStringLiteral("title")).toString(), .items = *items});
        }
        insights.periods.append(InsightsPeriod{.label = period.value(QStringLiteral("label")).toString(),
                                               .summary = period.value(QStringLiteral("summary")).toString(),
                                               .shares = *shares,
                                               .rankings = rankings});
    }
    return insights;
}

} // namespace

QJsonObject toJson(const LimitsReport &report) {
    QJsonArray windows;
    for (const UsageWindow &window : report.windows) {
        windows.append(QJsonObject{{QStringLiteral("label"), window.label},
                                   {QStringLiteral("kind"), kindName(window.kind)},
                                   {QStringLiteral("usedPercent"), window.usedPercent},
                                   {QStringLiteral("isBelowOne"), window.isBelowOne},
                                   {QStringLiteral("resetsAt"), dateTimeJson(window.resetsAt)}});
    }
    return {{QStringLiteral("provider"), report.provider},
            {QStringLiteral("windows"), windows},
            {QStringLiteral("billingNote"), optionalString(report.billingNote)},
            {QStringLiteral("insights"), report.insights ? QJsonValue(insightsJson(*report.insights)) : QJsonValue()},
            {QStringLiteral("rawOutput"), optionalString(report.rawOutput)}};
}

std::optional<LimitsReport> limitsReportFromJson(const QJsonObject &json) {
    if (!json.value(QStringLiteral("provider")).isString() || !json.value(QStringLiteral("windows")).isArray()) {
        return std::nullopt;
    }
    LimitsReport report{.provider = json.value(QStringLiteral("provider")).toString(),
                        .windows = {},
                        .billingNote = stringOrNull(json.value(QStringLiteral("billingNote"))),
                        .insights = std::nullopt,
                        .rawOutput = stringOrNull(json.value(QStringLiteral("rawOutput")))};
    for (const QJsonValue &item : json.value(QStringLiteral("windows")).toArray()) {
        const QJsonObject window = item.toObject();
        const auto kind = kindFromName(window.value(QStringLiteral("kind")).toString());
        if (!window.value(QStringLiteral("label")).isString() || !kind ||
            !window.value(QStringLiteral("usedPercent")).isDouble() ||
            !window.value(QStringLiteral("isBelowOne")).isBool()) {
            return std::nullopt;
        }
        std::optional<QDateTime> resetsAt;
        if (window.value(QStringLiteral("resetsAt")).isString()) {
            const QDateTime time =
                QDateTime::fromString(window.value(QStringLiteral("resetsAt")).toString(), Qt::ISODateWithMs);
            if (!time.isValid()) {
                return std::nullopt;
            }
            resetsAt = time;
        }
        report.windows.append(UsageWindow{.label = window.value(QStringLiteral("label")).toString(),
                                          .kind = *kind,
                                          .usedPercent = window.value(QStringLiteral("usedPercent")).toDouble(),
                                          .isBelowOne = window.value(QStringLiteral("isBelowOne")).toBool(),
                                          .resetsAt = resetsAt});
    }
    if (json.value(QStringLiteral("insights")).isObject()) {
        report.insights = insightsFrom(json.value(QStringLiteral("insights")).toObject());
        if (!report.insights) {
            return std::nullopt;
        }
    }
    return report;
}

QJsonObject toJson(const ActivityReport &report) {
    QJsonArray days;
    for (const ActivityDay &day : report.days) {
        QJsonObject byModel;
        for (auto it = day.byModel.cbegin(); it != day.byModel.cend(); ++it) {
            byModel.insert(it.key(), tokensJson(it.value()));
        }
        days.append(QJsonObject{{QStringLiteral("day"), day.day.toString(Qt::ISODate)},
                                {QStringLiteral("requests"), day.requests},
                                {QStringLiteral("sessions"), day.sessions},
                                {QStringLiteral("tokens"), tokensJson(day.tokens)},
                                {QStringLiteral("byModel"), byModel}});
    }
    return {{QStringLiteral("provider"), report.provider},
            {QStringLiteral("days"), days},
            {QStringLiteral("skippedLines"), report.skippedLines}};
}

std::optional<ActivityReport> activityReportFromJson(const QJsonObject &json) {
    if (!json.value(QStringLiteral("provider")).isString() || !json.value(QStringLiteral("days")).isArray() ||
        !json.value(QStringLiteral("skippedLines")).isDouble()) {
        return std::nullopt;
    }
    ActivityReport report{.provider = json.value(QStringLiteral("provider")).toString(),
                          .days = {},
                          .skippedLines = json.value(QStringLiteral("skippedLines")).toInteger()};
    for (const QJsonValue &item : json.value(QStringLiteral("days")).toArray()) {
        const QJsonObject day = item.toObject();
        const QDate date = QDate::fromString(day.value(QStringLiteral("day")).toString(), Qt::ISODate);
        const auto tokens = tokensFrom(day.value(QStringLiteral("tokens")));
        if (!date.isValid() || !tokens || !day.value(QStringLiteral("requests")).isDouble() ||
            !day.value(QStringLiteral("sessions")).isDouble() || !day.value(QStringLiteral("byModel")).isObject()) {
            return std::nullopt;
        }
        QMap<QString, TokenCounts> byModel;
        const QJsonObject models = day.value(QStringLiteral("byModel")).toObject();
        for (auto it = models.constBegin(); it != models.constEnd(); ++it) {
            const auto modelTokens = tokensFrom(it.value());
            if (!modelTokens) {
                return std::nullopt;
            }
            byModel.insert(it.key(), *modelTokens);
        }
        report.days.append(ActivityDay{.day = date,
                                       .requests = day.value(QStringLiteral("requests")).toInteger(),
                                       .sessions = day.value(QStringLiteral("sessions")).toInteger(),
                                       .tokens = *tokens,
                                       .byModel = byModel});
    }
    return report;
}

} // namespace contribusage
