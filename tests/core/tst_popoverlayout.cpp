#include "core/popoverlayout.h"

#include <QTest>

using namespace contribusage;

namespace {

const QString a = QStringLiteral("a");
const QString b = QStringLiteral("b");
const QString heatmap = heatmapBlockId();
const QString github = githubBlockId();
const QSet<QString> allOn{a, b, github};

using S = SectionKind;

} // namespace

// SPEC §16.3 popover layout and work follows use (FR-44 to FR-47, §10.8); ported from PopoverLayoutTests.swift.
class TestPopoverLayout : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void theDefaultOrderIsProvidersThenHeatmapThenGitHub() {
        QCOMPARE(PopoverLayout().blocks({a, b}, allOn), (QStringList{a, b, heatmap, github}));
    }

    // Review Focus 1, FR-45: a provider missing from the saved order comes last; an unknown one is kept, not shown.
    void aNewProviderIsAppendedAndUnknownKeysAreKept() {
        PopoverLayout layout;
        layout.order = {github, QStringLiteral("gone"), a, heatmap};
        layout.hiddenSections[QStringLiteral("gone")] = {S::Limits};
        const PopoverLayout saved = popoverLayoutFromText(popoverLayoutText(layout));
        QVERIFY(saved == layout);
        QCOMPARE(saved.blocks({a, b}, allOn), (QStringList{github, a, heatmap, b}));
    }

    // Review Focus 3.
    void brokenJsonIsTheDefault() {
        QVERIFY(popoverLayoutFromText(QStringLiteral("{not json")) == PopoverLayout());
        QVERIFY(popoverLayoutFromText(QString()) == PopoverLayout());
    }

    void hiddenAndOffBlocksAreLeftOut() {
        PopoverLayout layout;
        layout.hiddenBlocks = {a};
        QCOMPARE(layout.blocks({a, b}, {a, github}), (QStringList{heatmap, github}));
        layout.hiddenBlocks = {github, heatmap};
        QCOMPARE(layout.blocks({a, b}, {b}), QStringList{b});
        QVERIFY(layout.blocks({a, b}, {github}).isEmpty());
    }

    void theHeatmapNeedsALayerThatIsOn() {
        PopoverLayout layout;
        layout.outOfHeatmap = {a};
        QCOMPARE(layout.blocks({a}, {a}), QStringList{a});
        QCOMPARE(layout.blocks({a}, {a, github}), (QStringList{a, heatmap, github}));
    }

    void workFollowsUse() {
        PopoverLayout layout;
        layout.hiddenSections[a] = {S::Activity, S::Limits};
        layout.outOfHeatmap = {a};
        const Demand demand = layout.demand({a, b}, {});
        QCOMPARE(demand.activity, QSet<QString>{b});
        QVERIFY(!demand.github);
        layout.outOfHeatmap = {};
        QCOMPARE(layout.demand({a}, {}).activity, QSet<QString>{a});
        layout.hiddenBlocks = {heatmap};
        QVERIFY(layout.demand({a}, {}).activity.isEmpty());
        layout.hiddenBlocks = {heatmap, b};
        QVERIFY(layout.demand({b}, {}).activity.isEmpty());
        QCOMPARE(layout.demand({b}, {b}).activity, QSet<QString>{b});
        QVERIFY(layout.demand({}, {b}).activity.isEmpty());
    }

    void gitHubRunsWhileSomethingUsesIt() {
        PopoverLayout layout;
        layout.hiddenBlocks = {github};
        QVERIFY(layout.demand({github}, {}).github);
        layout.outOfHeatmap = {github};
        QVERIFY(!layout.demand({github}, {}).github);
        QVERIFY(layout.demand({github}, {github}).github);
        QVERIFY(!layout.demand({}, {github}).github);
    }

    void movingKeepsUnknownKeys() {
        PopoverLayout layout;
        layout.order = {QStringLiteral("gone"), github};
        layout.hiddenBlocks = {a};
        QCOMPARE(layout.arranged({a}), (QStringList{github, a, heatmap}));
        layout.move(heatmap, github, {a});
        QCOMPARE(layout.order, (QStringList{heatmap, github, a, QStringLiteral("gone")}));
        layout.move(github, a, {a});
        QCOMPARE(layout.order, (QStringList{heatmap, a, github, QStringLiteral("gone")}));
        QCOMPARE(layout.blocks({a}, {a, github}), (QStringList{heatmap, github}));
    }

    void aBlockIsOnWithItsSource() {
        PopoverLayout layout;
        layout.outOfHeatmap = {github};
        QVERIFY(layout.isOn(github, {github}));
        QVERIFY(!layout.isOn(heatmap, {github}));
        QVERIFY(layout.isOn(heatmap, {github, a}));
        QVERIFY(!layout.isOn(a, {github}));
    }

    void sectionsShowUntilHidden() {
        PopoverLayout layout;
        QVERIFY(layout.shows(S::Insights, a));
        layout.setShows(S::Insights, a, false);
        QVERIFY(!layout.shows(S::Insights, a) && layout.shows(S::Limits, a) && layout.shows(S::Insights, b));
        layout.setShows(S::Insights, a, true);
        QVERIFY(layout == PopoverLayout());
    }

    void sectionsMoveWithinTheirGroup() {
        PopoverLayout layout;
        QCOMPARE(layout.sections(a), (QList<S>{S::Limits, S::Activity, S::Insights}));
        layout.moveSection(S::Insights, S::Limits, a);
        QCOMPARE(layout.sections(a), (QList<S>{S::Insights, S::Limits, S::Activity}));
        layout.moveSection(S::Insights, S::Activity, a);
        QCOMPARE(layout.sections(a), (QList<S>{S::Limits, S::Activity, S::Insights}));
        layout.moveSection(S::Activity, S::Limits, a);
        QCOMPARE(layout.sections(a), (QList<S>{S::Activity, S::Limits, S::Insights}));
        QCOMPARE(layout.sections(b), (QList<S>{S::Limits, S::Activity, S::Insights}));
    }

    void heatmapLayersFollowBlockOrderAndMembership() {
        PopoverLayout layout;
        layout.order = {github, a};
        layout.outOfHeatmap = {b};
        QCOMPARE(layout.heatmapLayers({a, b}, {a, b, github}), (QStringList{github, a}));
    }
};

QTEST_GUILESS_MAIN(TestPopoverLayout)
#include "tst_popoverlayout.moc"
