#include "claudecode/usageparser.h"

#include <QRegularExpression>
#include <QStringList>

namespace contribusage::claudecode {

namespace {

// P-1.
QString stripAnsi(const QString &text) {
    static const QRegularExpression ansi(QStringLiteral("\x1b\\[[^A-Za-z]*[A-Za-z]"));
    return QString(text).remove(ansi);
}

QString capitalized(const QString &text) {
    return text.isEmpty() ? text : text.left(1).toUpper() + text.mid(1);
}

// "4:09am", "6 pm" → "4:09 AM", "6 PM", the form Qt's "AP" reads.
QString normalizedMeridiem(const QString &text) {
    static const QRegularExpression meridiem(QStringLiteral("\\s*([ap])m\\b"),
                                             QRegularExpression::CaseInsensitiveOption);
    QString result;
    qsizetype last = 0;
    auto matches = meridiem.globalMatch(text);
    while (matches.hasNext()) {
        const auto match = matches.next();
        result += text.mid(last, match.capturedStart() - last) + u' ' + match.captured(1).toUpper() + u'M';
        last = match.capturedEnd();
    }
    return result + text.mid(last);
}

// P-6 to P-9: nullopt when no format fits.
std::optional<QDateTime> resetDate(const QString &clause, const QDateTime &now, const QTimeZone &fallbackZone) {
    QString text = clause.trimmed();
    QTimeZone zone = fallbackZone;
    static const QRegularExpression zoned(QStringLiteral("^(.*?)\\s*\\(([^()]*)\\)$"));
    if (const auto match = zoned.match(text); match.hasMatch()) {
        text = match.captured(1);
        const QTimeZone named(match.captured(2).toUtf8());
        if (named.isValid()) {
            zone = named;
        }
    }
    text = normalizedMeridiem(text);

    // P-7 in order, with the comma forms of Claude Code 2.1.289 ("Oct 5, 6am").
    struct Format {
        const char *pattern;
        bool dated;
    };
    static constexpr Format formats[] = {
        {"MMM d 'at' h:mm AP", true},
        {"MMM d 'at' h AP", true},
        {"MMM d, h:mm AP", true},
        {"MMM d, h AP", true},
        {"MMM d", true},
        {"h:mm AP", false},
        {"h AP", false},
    };
    const QDateTime local = now.toTimeZone(zone);
    for (const Format &format : formats) {
        const QString pattern = QString::fromLatin1(format.pattern);
        if (!format.dated) {
            // P-8: a time only resolves to its next occurrence after now.
            const QTime time = QTime::fromString(text, pattern);
            if (!time.isValid()) {
                continue;
            }
            const QDateTime today(local.date(), time, zone);
            return today > now ? today : QDateTime(local.date().addDays(1), time, zone);
        }
        // P-8: this year, or next year when that lies more than 24 h in the past.
        const int year = local.date().year();
        for (const int candidate : {year, year + 1}) {
            const QDateTime parsed =
                QDateTime::fromString(text + u' ' + QString::number(candidate), pattern + QStringLiteral(" yyyy"));
            if (!parsed.isValid()) {
                continue;
            }
            const QDateTime result(parsed.date(), parsed.time(), zone);
            if (candidate != year || result >= now.addSecs(-24 * 3600)) {
                return result;
            }
        }
    }
    return std::nullopt;
}

// P-3 to P-5: nullopt for any other line.
std::optional<UsageWindow> window(const QString &line, const QDateTime &now, const QTimeZone &fallbackZone) {
    static const QRegularExpression pattern(
        QStringLiteral("^\\s*(?<label>[^:]+):\\s*(?<lt><)?\\s*(?<pct>\\d+(?:\\.\\d+)?)%\\s*used"
                       "(?:.*?\\bresets\\s+(?<reset>.+?))?\\s*$"));
    const auto match = pattern.match(line);
    if (!match.hasMatch()) {
        return std::nullopt;
    }
    const QString label = match.captured(u"label").trimmed();
    const bool isBelowOne = match.hasCaptured(u"lt");
    const QString reset = match.captured(u"reset");
    return UsageWindow{.label = label,
                       .kind = windowKind(label),
                       .usedPercent = isBelowOne ? 0.5 : match.captured(u"pct").toDouble(),
                       .isBelowOne = isBelowOne,
                       .resetsAt = reset.isEmpty() ? std::nullopt : resetDate(reset, now, fallbackZone)};
}

// "72% of your usage was at >150k context", "/skill-a 29%"; anything else keeps its text and no percent.
Share share(const QString &text) {
    static const QRegularExpression usage(
        QStringLiteral("^(?<pct>\\d+)% of your usage (?:was |came from )?(?<what>.+)$"));
    static const QRegularExpression item(QStringLiteral("^(?<name>.+) (?<pct>\\d+)%$"));
    if (const auto match = usage.match(text); match.hasMatch()) {
        return {.label = capitalized(match.captured(u"what")), .percent = match.captured(u"pct").toInt()};
    }
    if (const auto match = item.match(text); match.hasMatch()) {
        return {.label = match.captured(u"name"), .percent = match.captured(u"pct").toInt()};
    }
    return {.label = text, .percent = std::nullopt};
}

InsightsPeriod period(const QString &label, const QString &summary, const QStringList &lines) {
    static const QRegularExpression top(QStringLiteral("^Top (?<title>[^:]+):\\s*(?<items>.*)$"));
    InsightsPeriod result{.label = label, .summary = summary, .shares = {}, .rankings = {}};
    for (const QString &line : lines) {
        if (const auto match = top.match(line); match.hasMatch()) {
            Ranking ranking{.title = capitalized(match.captured(u"title")), .items = {}};
            for (const QString &entry : match.captured(u"items").split(QStringLiteral(", "), Qt::SkipEmptyParts)) {
                ranking.items.append(share(entry));
            }
            result.rankings.append(ranking);
        } else {
            result.shares.append(share(line));
        }
    }
    return result;
}

// P-11: after the heading, an unindented line with " · " opens a period, indented lines fill it, any other line joins
// the note. No period, no insights.
std::optional<Insights> insights(const QStringList &block) {
    static const QRegularExpression header(QStringLiteral("^(?<label>.+?) · (?<summary>.+)$"));
    QStringList note;
    struct Open {
        QString label;
        QString summary;
        QStringList lines;
    };
    QList<Open> periods;
    for (const QString &line : block.mid(1)) {
        const QString text = line.trimmed();
        if (text.isEmpty()) {
            continue;
        }
        if (line.front().isSpace() && !periods.isEmpty()) {
            periods.last().lines.append(text);
        } else if (const auto match = header.match(text); match.hasMatch()) {
            periods.append({.label = match.captured(u"label"), .summary = match.captured(u"summary"), .lines = {}});
        } else {
            note.append(text);
        }
    }
    if (periods.isEmpty()) {
        return std::nullopt;
    }
    Insights result{.note = note.isEmpty() ? std::nullopt : std::optional(note.join(u' ')), .periods = {}};
    for (const Open &open : periods) {
        result.periods.append(period(open.label, open.summary, open.lines));
    }
    return result;
}

} // namespace

WindowKind windowKind(const QString &label) {
    if (label == u"Current session") {
        return WindowKind::Session;
    }
    return label.startsWith(QStringLiteral("Current week")) ? WindowKind::Weekly : WindowKind::Other;
}

LimitsReport parseUsage(const QString &output, const QDateTime &now, const QTimeZone &fallbackZone) {
    const QStringList lines = stripAnsi(output).split(u'\n');
    LimitsReport report{.provider = providerId(),
                        .windows = {},
                        .billingNote = std::nullopt,
                        .insights = std::nullopt,
                        .rawOutput = output};
    static const QRegularExpression contributing(QStringLiteral("^[ \\t]*What['’]s contributing"));
    for (qsizetype index = 0; index < lines.size(); ++index) {
        const QString &line = lines.at(index);
        if (!report.billingNote && !line.trimmed().isEmpty()) {
            report.billingNote = line.trimmed(); // P-10
        }
        if (contributing.match(line).hasMatch()) {
            report.insights = insights(lines.mid(index));
            break;
        }
        if (auto parsed = window(line, now, fallbackZone)) {
            report.windows.append(*parsed);
        }
    }
    return report;
}

} // namespace contribusage::claudecode
