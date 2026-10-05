#include "claudecode/limitsview.h"

#include <QTest>
#include <QTimeZone>

using namespace contribusage;
using namespace contribusage::claudecode;

namespace {

const QDateTime now(QDate(2026, 10, 5), QTime(1, 0), QTimeZone::UTC);

Snapshot<LimitsReport> snapshot(qint64 ageSeconds) {
    const auto window = [](const char *label, WindowKind kind, double percent) {
        return UsageWindow{.label = QString::fromLatin1(label),
                           .kind = kind,
                           .usedPercent = percent,
                           .isBelowOne = false,
                           .resetsAt = now.addSecs(7800)};
    };
    return {.value =
                LimitsReport{.provider = QStringLiteral("claude-code"),
                             .windows = {window("Current session", WindowKind::Session, 23),
                                         window("Current week (all models)", WindowKind::Weekly, 74),
                                         window("Current week (Opus)", WindowKind::Weekly, 93)},
                             .billingNote = std::nullopt,
                             .insights =
                                 Insights{.note = QStringLiteral("Approximate."),
                                          .periods = {InsightsPeriod{
                                              .label = QStringLiteral("Last 24h"),
                                              .summary = QStringLiteral("5 requests"),
                                              .shares = {{.label = QStringLiteral("At >150k context"), .percent = 62}},
                                              .rankings = {{.title = QStringLiteral("Skills"),
                                                            .items = {{.label = QStringLiteral("/a"), .percent = 4},
                                                                      {.label = QStringLiteral("/b"), .percent = 3},
                                                                      {.label = QStringLiteral("/c"), .percent = 2},
                                                                      {.label = QStringLiteral("/d"),
                                                                       .percent = 1}}}}}}},
                             .rawOutput = std::nullopt},
            .fetchedAt = now.addSecs(-ageSeconds),
            .origin = origins::poll()};
}

} // namespace

class TestLimitsView : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void loadedShowsWindowsAndInsights() {
        const QVariantMap view = limitsView(SourceState<LimitsReport>::loaded(snapshot(120)), now);
        QCOMPARE(view.value(QStringLiteral("name")).toString(), QStringLiteral("Claude Code"));
        QCOMPARE(view.value(QStringLiteral("updatedText")).toString(), QStringLiteral("updated 2 min ago"));
        QCOMPARE(view.value(QStringLiteral("statusText")).toString(), QString());
        QVERIFY(!view.value(QStringLiteral("stale")).toBool());
        const QVariantList windows = view.value(QStringLiteral("windows")).toList();
        QCOMPARE(windows.size(), 2); // ADR-042: the Opus week is hidden
        QCOMPARE(windows.at(1).toMap().value(QStringLiteral("label")).toString(), QStringLiteral("Current week"));
        QCOMPARE(windows.at(1).toMap().value(QStringLiteral("level")).toString(), QStringLiteral("warning"));
        QCOMPARE(windows.at(0).toMap().value(QStringLiteral("resetText")).toString(),
                 QStringLiteral("resets in 2 hr 10 min"));
        const QVariantMap insights = view.value(QStringLiteral("insights")).toMap();
        const QVariantMap period = insights.value(QStringLiteral("periods")).toList().at(0).toMap();
        QCOMPARE(
            period.value(QStringLiteral("rows")).toList().at(0).toMap().value(QStringLiteral("percent")).toString(),
            QStringLiteral("62%"));
        const QVariantMap skills = period.value(QStringLiteral("rankings")).toList().at(0).toMap();
        QCOMPARE(skills.value(QStringLiteral("rows")).toList().size(), 3); // FR-38: the top three
    }

    void staleAfterThirtyMinutes() {
        QVERIFY(limitsView(SourceState<LimitsReport>::loaded(snapshot(31 * 60)), now)
                    .value(QStringLiteral("stale"))
                    .toBool());
    }

    // SPEC §11.3's texts.
    void statusTexts() {
        const auto status = [](const SourceState<LimitsReport> &state) {
            return limitsView(state, now).value(QStringLiteral("statusText")).toString();
        };
        QCOMPARE(status(SourceState<LimitsReport>::loading(std::nullopt)), QStringLiteral("Loading…"));
        QCOMPARE(status(SourceState<LimitsReport>::loading(snapshot(60))), QString());
        QCOMPARE(status(SourceState<LimitsReport>::notConfigured(NotConfiguredReason::ToolNotInstalled)),
                 QStringLiteral("Claude Code not found"));
        QCOMPARE(
            status(SourceState<LimitsReport>::notConfigured(NotConfiguredReason::UnsupportedPlan, QStringLiteral("x"))),
            QStringLiteral("Plan limits are only available when Claude Code uses a Claude subscription"));
        QCOMPARE(status(SourceState<LimitsReport>::failed({.kind = SourceError::Kind::NotLoggedIn}, snapshot(60))),
                 QStringLiteral("Claude Code isn't logged in"));
        const QVariantMap failed =
            limitsView(SourceState<LimitsReport>::failed({.kind = SourceError::Kind::TimedOut}, snapshot(60)), now);
        QCOMPARE(failed.value(QStringLiteral("windows")).toList().size(), 2); // previous values stay
        QVERIFY(failed.value(QStringLiteral("stale")).toBool());              // and are dimmed
    }

    // SPEC §11.1: the session's percent; "?" padded to two digits without data; "~" when stale.
    void labelShowsTheSessionOrUnknown() {
        const QVariantMap loaded = labelView(SourceState<LimitsReport>::loaded(snapshot(60)), now);
        QCOMPARE(loaded.value(QStringLiteral("text")).toString(), QStringLiteral("23%"));
        QCOMPARE(loaded.value(QStringLiteral("fraction")).toDouble(), 0.23);
        const QVariantMap unknown = labelView(SourceState<LimitsReport>::loading(std::nullopt), now);
        QCOMPARE(unknown.value(QStringLiteral("text")).toString(),
                 QString(QString(QChar(0x2007)).repeated(2) + QStringLiteral("?")));
        QCOMPARE(unknown.value(QStringLiteral("fraction")).toDouble(), 0.0);
        const QVariantMap stale = labelView(SourceState<LimitsReport>::loaded(snapshot(31 * 60)), now);
        QCOMPARE(stale.value(QStringLiteral("text")).toString(), QStringLiteral("~23%"));
    }

    // Review Focus 1: no data yet is a skeleton, without a status line.
    void firstLoadIsASkeleton() {
        const QVariantMap view = limitsView(SourceState<LimitsReport>::loading(std::nullopt), now);
        QVERIFY(view.value(QStringLiteral("skeleton")).toBool());
        QVERIFY(!limitsView(SourceState<LimitsReport>::loaded(snapshot(60)), now)
                     .value(QStringLiteral("skeleton"))
                     .toBool());
    }

    // Review Focus 2: SPEC §11.3, failed: an error line and Retry.
    void failureOffersRetry() {
        const QVariantMap view =
            limitsView(SourceState<LimitsReport>::failed({.kind = SourceError::Kind::TimedOut}, snapshot(60)), now);
        const QVariantMap action = view.value(QStringLiteral("action")).toMap();
        QCOMPARE(action.value(QStringLiteral("kind")).toString(), QStringLiteral("retry"));
        QCOMPARE(action.value(QStringLiteral("text")).toString(), QStringLiteral("Retry"));
    }

    // Review Focus 3: unparseable output can be shown.
    void unparseableOffersRawOutput() {
        const QVariantMap view = limitsView(
            SourceState<LimitsReport>::failed(
                {.kind = SourceError::Kind::Unparseable, .text = QStringLiteral("odd output")}, snapshot(60)),
            now);
        QCOMPARE(view.value(QStringLiteral("action")).toMap().value(QStringLiteral("kind")).toString(),
                 QStringLiteral("raw"));
        QCOMPARE(view.value(QStringLiteral("rawOutput")).toString(), QStringLiteral("odd output"));
    }

    void notFoundOffersLocate() {
        const QVariantMap view =
            limitsView(SourceState<LimitsReport>::notConfigured(NotConfiguredReason::ToolNotInstalled), now);
        const QVariantMap action = view.value(QStringLiteral("action")).toMap();
        QCOMPARE(action.value(QStringLiteral("kind")).toString(), QStringLiteral("configure"));
        QCOMPARE(action.value(QStringLiteral("text")).toString(), QStringLiteral("Locate…"));
        QVERIFY(!limitsView(SourceState<LimitsReport>::loaded(snapshot(60)), now).contains(QStringLiteral("action")));
    }

    // FR-51: what the label styles need besides the text.
    void labelCarriesTitleCompanionAndStale() {
        const QVariantMap label = labelView(SourceState<LimitsReport>::loaded(snapshot(60)), now);
        QCOMPARE(label.value(QStringLiteral("title")).toString(), QStringLiteral("Claude Code"));
        QVERIFY(label.value(QStringLiteral("companion")).toDouble() >= 0);
        QVERIFY(!label.value(QStringLiteral("stale")).toBool());
        QCOMPARE(label.value(QStringLiteral("source")).toString(), QStringLiteral("claude-code"));
        QCOMPARE(labelView(SourceState<LimitsReport>::loading(std::nullopt), now)
                     .value(QStringLiteral("companion"))
                     .toDouble(),
                 -1.0);
    }
};

QTEST_GUILESS_MAIN(TestLimitsView)
#include "tst_limitsview.moc"
