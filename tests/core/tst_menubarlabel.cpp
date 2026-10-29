#include "core/menubarlabel.h"

#include <QLocale>
#include <QTest>

using namespace contribusage;

namespace {

const QDateTime now(QDate(2026, 5, 9), QTime(12, 0), QTimeZone::UTC);
const QString pad = QString(QChar(0x2007));

MenuBarProvider limits(const QString &id, std::initializer_list<std::pair<WindowKind, double>> windows,
                       const QString &symbol = QStringLiteral("sparkle"), qint64 ageMs = 0) {
    LimitsReport report{.provider = id, .windows = {}, .billingNote = {}, .insights = {}, .rawOutput = {}};
    for (const auto &[kind, percent] : windows) {
        report.windows.append({.label = QStringLiteral("w"),
                               .kind = kind,
                               .usedPercent = percent,
                               .isBelowOne = false,
                               .resetsAt = now.addSecs(3600)});
    }
    return {.id = id,
            .displayName = id,
            .symbol = symbol,
            .staleAfterMs = 1'800'000,
            .limits =
                Snapshot<LimitsReport>{.value = report, .fetchedAt = now.addMSecs(-ageMs), .origin = origins::poll()}};
}

MenuBarLabel label(MenuBarMode mode, const QList<MenuBarProvider> &providers, const QString &provider = {},
                   std::optional<MenuBarGitHub> github = std::nullopt) {
    return menuBarLabel(mode, provider.isEmpty() && !providers.isEmpty() ? providers.first().id : provider, providers,
                        github, now);
}

} // namespace

// FR-12, SPEC §11.1, ported from MenuBarLabelTests.swift.
class TestMenuBarLabel : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void initTestCase() { QLocale::setDefault(QLocale(QLocale::English, QLocale::UnitedStates)); }

    void primaryAndWeeklyShowTheMenuBarProvidersWindow() {
        const auto claude = limits(QStringLiteral("a"),
                                   {{WindowKind::Session, 23}, {WindowKind::Weekly, 51}, {WindowKind::Weekly, 70}});
        const MenuBarLabel primary = label(MenuBarMode::Primary, {claude});
        QCOMPARE(primary.text, QStringLiteral("23%"));
        QCOMPARE(primary.meter, std::optional(0.23));
        QCOMPARE(primary.companion, std::optional(0.51));
        QCOMPARE(primary.source, QStringLiteral("a"));
        const MenuBarLabel weekly = label(MenuBarMode::Weekly, {claude});
        QCOMPARE(weekly.text, QStringLiteral("51%"));
        QCOMPARE(weekly.companion, std::optional(0.23));
        const auto other = limits(QStringLiteral("b"), {{WindowKind::Session, 5}});
        QCOMPARE(label(MenuBarMode::Primary, {claude, other}, QStringLiteral("b")).text,
                 QString(pad + QStringLiteral("5%")));
        QCOMPARE(label(MenuBarMode::Primary, {claude, other}, QStringLiteral("b")).companion, std::nullopt);
        QCOMPARE(label(MenuBarMode::Primary, {limits(QStringLiteral("a"), {{WindowKind::Session, 100}})}).text,
                 QStringLiteral("100%"));
    }

    // FR-11: a reset window fills nothing.
    void aResetWindowFillsNothing() {
        MenuBarProvider provider = limits(QStringLiteral("a"), {{WindowKind::Session, 80}});
        provider.limits->value.windows[0].resetsAt = now.addSecs(-1);
        QCOMPARE(label(MenuBarMode::Primary, {provider}).meter, std::nullopt);
    }

    void aMissingWindowFallsBackToTheHighestThenToUnknown() {
        const auto first = limits(QStringLiteral("a"), {{WindowKind::Weekly, 40}});
        const auto second = limits(QStringLiteral("b"), {{WindowKind::Session, 12}, {WindowKind::Weekly, 77}},
                                   QStringLiteral("terminal"));
        const MenuBarLabel highest = label(MenuBarMode::Primary, {first, second});
        QCOMPARE(highest.text, QStringLiteral("77%"));
        QCOMPARE(highest.title, QStringLiteral("b"));
        QCOMPARE(highest.badge, QStringLiteral("terminal"));
        QCOMPARE(highest.companion, std::optional(0.12));
        QCOMPARE(label(MenuBarMode::Highest, {first}).badge, QString());
        MenuBarProvider empty = limits(QStringLiteral("a"), {});
        empty.limits.reset();
        const MenuBarLabel unknown = label(MenuBarMode::Primary, {empty});
        QCOMPARE(unknown.text, QString(pad + pad + QStringLiteral("?")));
        QCOMPARE(unknown.meter, std::nullopt);
        QCOMPARE(label(MenuBarMode::Weekly, {limits(QStringLiteral("a"), {{WindowKind::Session, 5}})}).text,
                 QString(pad + QStringLiteral("5%")));
    }

    void staleValuesAreMarked() {
        const MenuBarLabel stale =
            label(MenuBarMode::Primary,
                  {limits(QStringLiteral("a"), {{WindowKind::Session, 23}}, QStringLiteral("s"), 1'801'000)});
        QCOMPARE(stale.text, QStringLiteral("23%"));
        QVERIFY(stale.stale);
        const MenuBarLabel github = label(MenuBarMode::GithubToday, {}, {},
                                          MenuBarGitHub{.today = 7, .level = 2, .activeDays = 3, .stale = true});
        QCOMPARE(github.text, QStringLiteral("7"));
        QVERIFY(github.stale);
    }

    void gitHubModesShowTodaysContributions() {
        const auto first = limits(QStringLiteral("a"), {{WindowKind::Session, 23}});
        const MenuBarGitHub github{.today = 7, .level = 2, .activeDays = 3, .stale = false};
        const MenuBarLabel today = label(MenuBarMode::GithubToday, {first}, {}, github);
        QCOMPARE(today.text, QStringLiteral("7"));
        QCOMPARE(today.title, QStringLiteral("GitHub"));
        QCOMPARE(today.meter, std::optional(0.5));
        QCOMPARE(today.source, QStringLiteral("github"));
        QCOMPARE(label(MenuBarMode::GithubToday, {first}).text, QStringLiteral("?"));
        const MenuBarLabel both = label(MenuBarMode::PrimaryAndGitHub, {first}, {}, github);
        QCOMPARE(both.text, QStringLiteral("23% · 7"));
        QCOMPARE(both.meter, std::optional(0.23));
    }

    void iconOnlyKeepsTheMeters() {
        const MenuBarLabel icon =
            label(MenuBarMode::IconOnly,
                  {limits(QStringLiteral("a"), {{WindowKind::Session, 23}, {WindowKind::Weekly, 51}})});
        QCOMPARE(icon.text, QString());
        QCOMPARE(icon.meter, std::optional(0.23));
        QCOMPARE(label(MenuBarMode::IconOnly, {}).title, QStringLiteral("contribusage"));
    }

    void onlyModesOfSourcesThatAreOnAreOffered() {
        using M = MenuBarMode;
        QCOMPARE(offeredModes(true, true),
                 (QList<M>{M::Primary, M::Weekly, M::Highest, M::GithubToday, M::PrimaryAndGitHub, M::IconOnly}));
        QCOMPARE(offeredModes(true, false), (QList<M>{M::Primary, M::Weekly, M::Highest, M::IconOnly}));
        QCOMPARE(offeredModes(false, true), (QList<M>{M::GithubToday, M::IconOnly}));
        QCOMPARE(resolvedMode(M::PrimaryAndGitHub, offeredModes(true, false)), M::Primary);
        QCOMPARE(resolvedMode(M::Weekly, offeredModes(false, true)), M::GithubToday);
        QCOMPARE(resolvedMode(M::GithubToday, offeredModes(false, false)), M::IconOnly);
        QCOMPARE(resolvedMode(M::Weekly, offeredModes(true, false)), M::Weekly);
    }
};

QTEST_GUILESS_MAIN(TestMenuBarLabel)
#include "tst_menubarlabel.moc"
