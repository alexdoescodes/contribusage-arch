#include "core/display.h"
#include "core/heatmap.h"

#include <QColor>
#include <QDir>
#include <QFile>
#include <QGuiApplication>
#include <QPalette>
#include <QPluginLoader>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickWindow>
#include <QRegularExpression>
#include <QTemporaryDir>
#include <QTest>

#include <functional>
#include <memory>

// Repeater delegates have only a visual parent, so QObject::findChild does not reach them.
QQuickItem *findItem(QObject *root, const QString &name) {
    auto *item = qobject_cast<QQuickItem *>(root);
    if (!item) {
        return nullptr;
    }
    if (item->objectName() == name) {
        return item;
    }
    for (QQuickItem *child : item->childItems()) {
        if (QQuickItem *found = findItem(child, name)) {
            return found;
        }
    }
    return nullptr;
}

// Loads the built applet and compiles its QML the way plasmashell would: a QML error or a missing type fails here,
// not as an empty widget in the panel.
class TestAppletQml : public QObject {
    Q_OBJECT

    static constexpr auto uri = "plasma.applet.org.contribusage.widget";
    QPluginLoader m_loader{QStringLiteral(CONTRIBUSAGE_APPLET_PATH)};

private Q_SLOTS:
    void initTestCase() {
        // The applet tests read the macOS preview data; live mode would run the real claude.
        qputenv("CONTRIBUSAGE_FAKE_DATA", "1");
        QVERIFY2(m_loader.load(), qPrintable(m_loader.errorString()));
    }

    void qmlFileCompiles_data() {
        QTest::addColumn<QString>("file");
        for (const auto *name : {"main",          "Theme",           "SectionHeader",
                                 "LimitBar",      "ActivitySection", "InsightsSection",
                                 "ProviderGroup", "HeatmapBlock",    "GitHubSection",
                                 "Footer",        "FullPopup",       "PromptGlyph",
                                 "CompactLabel",  "Skeleton",        "ActionButton",
                                 "LabelContent",  "RingGlyph",       "LineGlyph",
                                 "MiniHeatmap",   "ConfigGeneral",   "ConfigClaudeCode",
                                 "ConfigGitHub",  "ConfigAdvanced"}) {
            QTest::newRow(name) << QString::fromLatin1(name);
        }
    }
    void qmlFileCompiles() {
        QFETCH(QString, file);
        QQmlEngine engine;
        QQmlComponent component(
            &engine, QUrl(QStringLiteral("qrc:/qt/qml/plasma/applet/org/contribusage/widget/%1.qml").arg(file)));
        QVERIFY2(component.status() == QQmlComponent::Ready, qPrintable(component.errorString()));
    }

    void appInfoSingletonReportsTheVersion() {
        QQmlEngine engine;
        auto *appInfo = engine.singletonInstance<QObject *>(QString::fromLatin1(uri), QStringLiteral("AppInfo"));
        QVERIFY(appInfo);
        QCOMPARE(appInfo->property("version").toString(), QStringLiteral("0.1.0"));
    }

    void popupModelHoldsTheMockLimits() {
        QQmlEngine engine;
        auto *model = engine.singletonInstance<QObject *>(QString::fromLatin1(uri), QStringLiteral("PopupModel"));
        QVERIFY(model);
        const auto providers = model->property("providers").toList();
        QCOMPARE(providers.size(), 1);
        const auto claude = providers.first().toMap();
        QCOMPARE(claude.value(QStringLiteral("name")).toString(), QStringLiteral("Claude Code"));
        QCOMPARE(claude.value(QStringLiteral("updatedText")).toString(), QStringLiteral("updated 2 min ago"));
        const auto windows = claude.value(QStringLiteral("windows")).toList();
        // ADR-042: session and one week bar; the Opus week window of the mock is hidden.
        QCOMPARE(windows.size(), 2);
        const auto session = windows.at(0).toMap();
        QCOMPARE(session.value(QStringLiteral("percentText")).toString(), QStringLiteral("23%"));
        QCOMPARE(session.value(QStringLiteral("level")).toString(), QStringLiteral("normal"));
        QCOMPARE(session.value(QStringLiteral("resetText")).toString(), QStringLiteral("resets in 2 hr 10 min"));
        const auto week = windows.at(1).toMap();
        QCOMPARE(week.value(QStringLiteral("label")).toString(), QStringLiteral("Current week"));
        QCOMPARE(week.value(QStringLiteral("level")).toString(), QStringLiteral("warning"));
    }

    void popupModelHoldsTheMockActivity() {
        QQmlEngine engine;
        auto *model = engine.singletonInstance<QObject *>(QString::fromLatin1(uri), QStringLiteral("PopupModel"));
        const auto activity =
            model->property("providers").toList().first().toMap().value(QStringLiteral("activity")).toMap();
        QCOMPARE(activity.value(QStringLiteral("todayText")).toString(),
                 QStringLiteral("Today: 668 requests · 8 sessions · 5.3M"));
        QCOMPARE(activity.value(QStringLiteral("categoriesText")).toString(),
                 QStringLiteral("Input 12K · Output 812K · Cache read 4.2M · Cache write 300K"));
        const auto week = activity.value(QStringLiteral("week")).toList();
        QCOMPARE(week.size(), 7);
        QVERIFY(week.last().toMap().value(QStringLiteral("today")).toBool());
    }

    void popupModelHeatmapCoversTheShownRange() {
        QQmlEngine engine;
        auto *model = engine.singletonInstance<QObject *>(QString::fromLatin1(uri), QStringLiteral("PopupModel"));
        const auto heatmap = model->property("heatmap").toMap();
        const auto days = contribusage::heatmapDays(QDate::currentDate());
        QCOMPARE(heatmap.value(QStringLiteral("cells")).toList().size(), days.size());
        QCOMPARE(heatmap.value(QStringLiteral("columns")).toInt(), (days.size() + 6) / 7);
        QCOMPARE(heatmap.value(QStringLiteral("layers")).toList().size(), 2);
        const auto today = heatmap.value(QStringLiteral("cells")).toList().last().toMap();
        QVERIFY(today.value(QStringLiteral("line")).toString().startsWith(QDate::currentDate().toString(Qt::ISODate)));
    }

    void heatmapLineReadsNoData() {
        QQmlEngine engine;
        auto *model = engine.singletonInstance<QObject *>(QString::fromLatin1(uri), QStringLiteral("PopupModel"));
        // The mock Claude Code layer starts 120 days ago (FR-48: earlier days are "no data").
        const auto first = model->property("heatmap").toMap().value(QStringLiteral("cells")).toList().first().toMap();
        QVERIFY2(first.value(QStringLiteral("line")).toString().contains(QStringLiteral("Claude Code: no data")),
                 qPrintable(first.value(QStringLiteral("line")).toString()));
    }

    void popupModelLabelShowsTheSession() {
        QQmlEngine engine;
        auto *model = engine.singletonInstance<QObject *>(QString::fromLatin1(uri), QStringLiteral("PopupModel"));
        const auto label = model->property("label").toMap();
        QCOMPARE(label.value(QStringLiteral("text")).toString(), QStringLiteral("23%"));
        QCOMPARE(label.value(QStringLiteral("fraction")).toDouble(), 0.23);
    }

    void themeSingletonResolves() {
        QQmlEngine engine;
        QQmlComponent component(&engine);
        component.setData("import QtQuick\nimport plasma.applet.org.contribusage.widget\nQtObject { "
                          "property int width: Theme.popupWidth }",
                          QUrl());
        std::unique_ptr<QObject> object(component.create());
        QVERIFY2(object, qPrintable(component.errorString()));
        QCOMPARE(object->property("width").toInt(), 360);
    }

    void insightsTabsShareOneRow() {
        QQmlEngine engine;
        auto *model = engine.singletonInstance<QObject *>(QString::fromLatin1(uri), QStringLiteral("PopupModel"));
        const auto insights = model->property("providers").toList().first().toMap().value(QStringLiteral("insights"));
        QQuickWindow window;
        QQmlComponent component(
            &engine, QUrl(QStringLiteral("qrc:/qt/qml/plasma/applet/org/contribusage/widget/InsightsSection.qml")));
        std::unique_ptr<QObject> object(component.createWithInitialProperties(
            {{QStringLiteral("insights"), insights}, {QStringLiteral("expanded"), true}}));
        QVERIFY2(object, qPrintable(component.errorString()));
        auto *section = qobject_cast<QQuickItem *>(object.get());
        section->setParentItem(window.contentItem());
        section->setWidth(360);
        window.resize(360, 400);
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        QList<QQuickItem *> tabs;
        std::function<void(QQuickItem *)> collect = [&](QQuickItem *item) {
            if (QByteArray(item->metaObject()->className()).contains("TabButton")) {
                tabs.append(item);
            }
            for (auto *child : item->childItems()) {
                collect(child);
            }
        };
        collect(section);
        QCOMPARE(tabs.size(), 2);
        QTRY_COMPARE(tabs.at(0)->height(), tabs.at(1)->height());
        QCOMPARE(tabs.at(0)->mapToScene({}).y(), tabs.at(1)->mapToScene({}).y());
    }

    void popupModelNotifiesChanges() {
        QQmlEngine engine;
        auto *model = engine.singletonInstance<QObject *>(QString::fromLatin1(uri), QStringLiteral("PopupModel"));
        QVERIFY(model);
        QVERIFY(model->metaObject()->indexOfSignal("changed()") >= 0);
        const auto property = model->metaObject()->property(model->metaObject()->indexOfProperty("providers"));
        QVERIFY(property.hasNotifySignal());
    }

    // Live mode rebuilds the provider rows every minute; the open Insights and the chosen period must survive that.
    void insightsStateSurvivesRecreation() {
        QQmlEngine engine;
        auto *model = engine.singletonInstance<QObject *>(QString::fromLatin1(uri), QStringLiteral("PopupModel"));
        const auto insights = model->property("providers").toList().first().toMap().value(QStringLiteral("insights"));
        QQmlComponent stateComponent(&engine);
        stateComponent.setData("import QtQuick\nQtObject { property bool expanded: false; property int period: 0 }",
                               QUrl());
        std::unique_ptr<QObject> state(stateComponent.create());
        QVERIFY(state);
        QQmlComponent section(
            &engine, QUrl(QStringLiteral("qrc:/qt/qml/plasma/applet/org/contribusage/widget/InsightsSection.qml")));
        const QVariantMap properties{{QStringLiteral("insights"), insights},
                                     {QStringLiteral("viewState"), QVariant::fromValue(state.get())}};
        {
            std::unique_ptr<QObject> first(section.createWithInitialProperties(properties));
            QVERIFY2(first, qPrintable(section.errorString()));
            QVERIFY(QMetaObject::invokeMethod(first.get(), "toggle"));
            QVERIFY(QMetaObject::invokeMethod(first.get(), "selectPeriod", Q_ARG(QVariant, QVariant(1))));
        }
        std::unique_ptr<QObject> second(section.createWithInitialProperties(properties));
        QVERIFY(second);
        QCOMPARE(second->property("expanded").toBool(), true);
        QCOMPARE(second->property("period").toInt(), 1);
    }

    // Live mode's not-connected block: the form is there, and submitting clears the field (Review Focus 4).
    void githubFormClearsTheToken() {
        QQmlEngine engine;
        QQmlComponent component(
            &engine, QUrl(QStringLiteral("qrc:/qt/qml/plasma/applet/org/contribusage/widget/GitHubSection.qml")));
        const QVariantMap github{{QStringLiteral("connected"), false},       {QStringLiteral("login"), QString()},
                                 {QStringLiteral("statsText"), QString()},   {QStringLiteral("updatedText"), QString()},
                                 {QStringLiteral("statusText"), QString()},  {QStringLiteral("busy"), false},
                                 {QStringLiteral("connectError"), QString()}};
        std::unique_ptr<QObject> section(component.createWithInitialProperties({{QStringLiteral("github"), github}}));
        QVERIFY2(section, qPrintable(component.errorString()));
        auto *field = section->findChild<QObject *>(QStringLiteral("tokenField"));
        QVERIFY(field);
        field->setProperty("text", QStringLiteral("ghp_typed"));
        QVERIFY(QMetaObject::invokeMethod(section.get(), "submit"));
        QCOMPARE(field->property("text").toString(), QString());
    }

    // SPEC §11.2: the footer's Refresh is live.
    void refreshButtonIsEnabled() {
        QQmlEngine engine;
        QQmlComponent component(&engine,
                                QUrl(QStringLiteral("qrc:/qt/qml/plasma/applet/org/contribusage/widget/Footer.qml")));
        std::unique_ptr<QObject> footer(component.create());
        QVERIFY2(footer, qPrintable(component.errorString()));
        auto *refresh = footer->findChild<QObject *>(QStringLiteral("refreshButton"));
        QVERIFY(refresh);
        QVERIFY(refresh->property("enabled").toBool());
    }

    // Review Focus 1: the first load shows skeleton bars.
    void skeletonShowsWhileLoading() {
        QQmlEngine engine;
        QQmlComponent component(
            &engine, QUrl(QStringLiteral("qrc:/qt/qml/plasma/applet/org/contribusage/widget/ProviderGroup.qml")));
        const QVariantMap provider{{QStringLiteral("name"), QStringLiteral("Claude Code")},
                                   {QStringLiteral("symbol"), QStringLiteral("draw-star")},
                                   {QStringLiteral("updatedText"), QString()},
                                   {QStringLiteral("statusText"), QString()},
                                   {QStringLiteral("stale"), false},
                                   {QStringLiteral("skeleton"), true},
                                   {QStringLiteral("windows"), QVariantList()}};
        std::unique_ptr<QObject> group(component.createWithInitialProperties({{QStringLiteral("provider"), provider}}));
        QVERIFY2(group, qPrintable(component.errorString()));
        auto *skeleton = findItem(group.get(), QStringLiteral("skeleton"));
        QVERIFY(skeleton);
        QVERIFY(skeleton->property("visible").toBool());
    }

    // Review Focus 2: a failure shows its action.
    void retryButtonShows() {
        QQmlEngine engine;
        QQmlComponent component(
            &engine, QUrl(QStringLiteral("qrc:/qt/qml/plasma/applet/org/contribusage/widget/ProviderGroup.qml")));
        const QVariantMap provider{
            {QStringLiteral("name"), QStringLiteral("Claude Code")},
            {QStringLiteral("symbol"), QStringLiteral("draw-star")},
            {QStringLiteral("updatedText"), QString()},
            {QStringLiteral("statusText"), QStringLiteral("Claude Code didn't answer in time")},
            {QStringLiteral("stale"), true},
            {QStringLiteral("skeleton"), false},
            {QStringLiteral("action"), QVariantMap{{QStringLiteral("kind"), QStringLiteral("retry")},
                                                   {QStringLiteral("text"), QStringLiteral("Retry")}}},
            {QStringLiteral("windows"), QVariantList()}};
        std::unique_ptr<QObject> group(component.createWithInitialProperties({{QStringLiteral("provider"), provider}}));
        QVERIFY2(group, qPrintable(component.errorString()));
        auto *button = findItem(group.get(), QStringLiteral("actionButton"));
        QVERIFY(button);
        QVERIFY(button->property("visible").toBool());
        QCOMPARE(button->property("text").toString(), QStringLiteral("Retry"));
    }

    QObject *labelContent(QQmlEngine &engine, const QString &style, std::unique_ptr<QObject> &holder) {
        QQmlComponent component(
            &engine, QUrl(QStringLiteral("qrc:/qt/qml/plasma/applet/org/contribusage/widget/LabelContent.qml")));
        auto *model = engine.singletonInstance<QObject *>(QString::fromLatin1(uri), QStringLiteral("PopupModel"));
        holder.reset(component.createWithInitialProperties({{QStringLiteral("style"), style},
                                                            {QStringLiteral("tint"), QStringLiteral("provider")},
                                                            {QStringLiteral("label"), model->property("label")},
                                                            {QStringLiteral("heatmap"), model->property("heatmap")},
                                                            {QStringLiteral("vertical"), false}}));
        if (!holder) {
            qWarning() << component.errorString();
        }
        return holder.get();
    }

    // FR-51: every style draws.
    void everyStyleDraws_data() {
        QTest::addColumn<QString>("style");
        for (const auto *style : {"prompt", "rings", "ring", "line", "heatmap", "sharedHeatmap", "text"}) {
            QTest::newRow(style) << QString::fromLatin1(style);
        }
    }
    void everyStyleDraws() {
        QFETCH(QString, style);
        QQmlEngine engine;
        std::unique_ptr<QObject> holder;
        QObject *content = labelContent(engine, style, holder);
        QVERIFY(content);
        QCOMPARE(content->property("shownStyle").toString(), style);
        QVERIFY(content->property("implicitWidth").toReal() > 0);
    }

    // FR-51 text style: the source's name and the value.
    void textStyleNamesTheSource() {
        QQmlEngine engine;
        std::unique_ptr<QObject> holder;
        QObject *content = labelContent(engine, QStringLiteral("text"), holder);
        QVERIFY(content);
        auto *text = content->findChild<QObject *>(QStringLiteral("labelText"));
        QVERIFY(text);
        QCOMPARE(text->property("text").toString(), QStringLiteral("Claude Code 23%"));
    }

    // Review Focus 4: an unknown style draws the prompt.
    void unknownStyleFallsBackToPrompt() {
        QQmlEngine engine;
        std::unique_ptr<QObject> holder;
        QObject *content = labelContent(engine, QStringLiteral("sparkles"), holder);
        QVERIFY(content);
        QCOMPARE(content->property("shownStyle").toString(), QStringLiteral("prompt"));
    }

    // Every style laid out in a real window keeps a panel's size: a size loop here would freeze plasmashell.
    void everyStyleStaysSmallInAWindow() {
        QFETCH(QString, style);
        QQmlEngine engine;
        std::unique_ptr<QObject> holder;
        auto *content = qobject_cast<QQuickItem *>(labelContent(engine, style, holder));
        QVERIFY(content);
        QQuickWindow window;
        content->setParentItem(window.contentItem());
        window.resize(300, 40);
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        QTest::qWait(50);
        QVERIFY2(content->implicitHeight() < 64, qPrintable(QString::number(content->implicitHeight())));
    }
    void everyStyleStaysSmallInAWindow_data() { everyStyleDraws_data(); }

    // Review Focus 5, SPEC §11.7: arrows walk the grid and stay inside it; the line follows.
    void arrowKeysWalkTheGrid() {
        QQmlEngine engine;
        auto *model = engine.singletonInstance<QObject *>(QString::fromLatin1(uri), QStringLiteral("PopupModel"));
        const QVariantMap heatmap = model->property("heatmap").toMap();
        const QVariantList cells = heatmap.value(QStringLiteral("cells")).toList();
        QQmlComponent component(
            &engine, QUrl(QStringLiteral("qrc:/qt/qml/plasma/applet/org/contribusage/widget/HeatmapBlock.qml")));
        QQuickWindow window;
        std::unique_ptr<QObject> object(component.createWithInitialProperties({{QStringLiteral("heatmap"), heatmap}}));
        QVERIFY2(object, qPrintable(component.errorString()));
        auto *block = qobject_cast<QQuickItem *>(object.get());
        block->setParentItem(window.contentItem());
        block->setWidth(340);
        window.resize(360, 300);
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        auto *grid = block->findChild<QQuickItem *>(QStringLiteral("heatmapGrid"));
        QVERIFY(grid);
        grid->forceActiveFocus();
        const int last = int(cells.size()) - 1;
        QTRY_COMPARE(block->property("focusedIndex").toInt(), last);
        QTest::keyClick(&window, Qt::Key_Left);
        QCOMPARE(block->property("focusedIndex").toInt(), last - 7);
        QTest::keyClick(&window, Qt::Key_Up);
        QCOMPARE(block->property("focusedIndex").toInt(), last - 8);
        auto *line = block->findChild<QObject *>(QStringLiteral("heatmapLine"));
        QCOMPARE(line->property("text").toString(),
                 cells.at(last - 8).toMap().value(QStringLiteral("line")).toString());
        QTest::keyClick(&window, Qt::Key_End);
        QTest::keyClick(&window, Qt::Key_Right);
        QCOMPARE(block->property("focusedIndex").toInt(), last); // stays inside
        QTest::keyClick(&window, Qt::Key_Home);
        QTest::keyClick(&window, Qt::Key_Up);
        QCOMPARE(block->property("focusedIndex").toInt(), 0);
    }

    // Review I-3: a vertical panel in the text style still shows something to click.
    void textStyleShowsAGlyphInAVerticalPanel() {
        QQmlEngine engine;
        std::unique_ptr<QObject> holder;
        QObject *content = labelContent(engine, QStringLiteral("text"), holder);
        QVERIFY(content);
        content->setProperty("vertical", true);
        qsizetype shown = 0;
        for (QQuickItem *child : qobject_cast<QQuickItem *>(content)->childItems()) {
            shown += child->isVisible();
        }
        QVERIFY(shown > 0);
    }

    // Review I-1: fewer cells after a refresh keep the focused day inside the grid, without script errors.
    void focusSurvivesFewerCells() {
        QQmlEngine engine;
        auto *model = engine.singletonInstance<QObject *>(QString::fromLatin1(uri), QStringLiteral("PopupModel"));
        QVariantMap heatmap = model->property("heatmap").toMap();
        QQmlComponent component(
            &engine, QUrl(QStringLiteral("qrc:/qt/qml/plasma/applet/org/contribusage/widget/HeatmapBlock.qml")));
        QQuickWindow window;
        std::unique_ptr<QObject> object(component.createWithInitialProperties({{QStringLiteral("heatmap"), heatmap}}));
        QVERIFY2(object, qPrintable(component.errorString()));
        auto *block = qobject_cast<QQuickItem *>(object.get());
        block->setParentItem(window.contentItem());
        block->setWidth(340);
        window.resize(360, 300);
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        block->findChild<QQuickItem *>(QStringLiteral("heatmapGrid"))->forceActiveFocus();
        QVariantList cells = heatmap.value(QStringLiteral("cells")).toList();
        QTRY_COMPARE(block->property("focusedIndex").toInt(), int(cells.size()) - 1);
        QTest::failOnWarning(QRegularExpression(QStringLiteral(".*TypeError.*")));
        cells.removeLast();
        cells.removeLast();
        heatmap.insert(QStringLiteral("cells"), cells);
        block->setProperty("heatmap", heatmap);
        QCOMPARE(block->property("focusedIndex").toInt(), int(cells.size()) - 1);
        auto *line = block->findChild<QObject *>(QStringLiteral("heatmapLine"));
        QCOMPARE(line->property("text").toString(), cells.last().toMap().value(QStringLiteral("line")).toString());
    }

    // SPEC §11.6: the configuration dialog's categories.
    void configModelListsTheTabs() {
        QFile file(QStringLiteral(":/qt/qml/plasma/applet/org/contribusage/widget/config.qml"));
        QVERIFY(file.open(QIODevice::ReadOnly));
        const QByteArray text = file.readAll();
        for (const char *page :
             {"ConfigGeneral.qml", "ConfigClaudeCode.qml", "ConfigGitHub.qml", "ConfigAdvanced.qml"}) {
            QVERIFY2(text.contains(page), page);
        }
    }

    // The schema holds every SPEC §10.7 key the pages edit.
    void schemaHoldsTheSettings() {
        QFile file(QStringLiteral(":/qt/qml/plasma/applet/org/contribusage/widget/main.xml"));
        QVERIFY(file.open(QIODevice::ReadOnly));
        const QByteArray text = file.readAll();
        for (const char *key :
             {"menuBarMode", "menuBarStyle", "menuBarTint", "menuBarColor", "notificationThresholds", "notifyOnReset",
              "probeIntervalMinutes", "claudePathOverride", "githubEnabled", "githubIntervalMinutes"}) {
            QVERIFY2(text.contains(QByteArray(QByteArray("name=\"") + key + '"')), key);
        }
    }

    // FR-12: the fake model follows the configured mode too.
    void applySettingsIsCallable() {
        QQmlEngine engine;
        auto *model = engine.singletonInstance<QObject *>(QString::fromLatin1(uri), QStringLiteral("PopupModel"));
        QVERIFY(QMetaObject::invokeMethod(
            model, "applySettings",
            Q_ARG(QVariantMap, (QVariantMap{{QStringLiteral("menuBarMode"), QStringLiteral("weekly")}}))));
        QVERIFY(!model->property("dataFolder").toString().isEmpty());
    }

    QObject *model(QQmlEngine &engine) {
        return engine.singletonInstance<QObject *>(QString::fromLatin1(uri), QStringLiteral("PopupModel"));
    }

    // FR-44: the popup's blocks follow the saved layout.
    void blocksFollowTheLayout() {
        QQmlEngine engine;
        QObject *popup = model(engine);
        QCOMPARE(popup->property("blocks").toStringList(),
                 (QStringList{QStringLiteral("claude-code"), QStringLiteral("heatmap"), QStringLiteral("github")}));
        QMetaObject::invokeMethod(
            popup, "applySettings",
            Q_ARG(QVariantMap, (QVariantMap{{QStringLiteral("popoverLayout"),
                                             QStringLiteral(R"({"order":["github"],"hiddenBlocks":["heatmap"]})")}})));
        QCOMPARE(popup->property("blocks").toStringList(),
                 (QStringList{QStringLiteral("github"), QStringLiteral("claude-code")}));
        QMetaObject::invokeMethod(popup, "applySettings", Q_ARG(QVariantMap, QVariantMap()));
    }

    // Review Focus 2, FR-47.
    void nothingToShowWhenEverythingIsHidden() {
        QQmlEngine engine;
        QObject *popup = model(engine);
        QVERIFY(!popup->property("nothingToShow").toBool());
        QMetaObject::invokeMethod(
            popup, "applySettings",
            Q_ARG(QVariantMap,
                  (QVariantMap{{QStringLiteral("popoverLayout"),
                                QStringLiteral(R"({"hiddenBlocks":["claude-code","heatmap","github"]})")}})));
        QVERIFY(popup->property("nothingToShow").toBool());
        QMetaObject::invokeMethod(popup, "applySettings", Q_ARG(QVariantMap, QVariantMap()));
    }

    // FR-44, ADR-034: a provider's sections in order, hidden ones left out.
    void sectionsFollowTheLayout() {
        QQmlEngine engine;
        QObject *popup = model(engine);
        QMetaObject::invokeMethod(
            popup, "applySettings",
            Q_ARG(
                QVariantMap,
                (QVariantMap{
                    {QStringLiteral("popoverLayout"),
                     QStringLiteral(
                         R"({"hiddenSections":{"claude-code":["insights"]},"sectionOrder":{"claude-code":["activity","limits"]}})")}})));
        const QVariantMap claude = popup->property("providers").toList().first().toMap();
        QCOMPARE(claude.value(QStringLiteral("sections")).toStringList(),
                 (QStringList{QStringLiteral("activity"), QStringLiteral("limits")}));
        QMetaObject::invokeMethod(popup, "applySettings", Q_ARG(QVariantMap, QVariantMap()));
    }

    // SPEC §11.6 Popover tab: the helpers edit the saved layout text.
    void layoutHelpersEditTheText() {
        QQmlEngine engine;
        QObject *popup = model(engine);
        QString layout;
        QVariantList rows;
        QMetaObject::invokeMethod(popup, "layoutRows", Q_RETURN_ARG(QVariantList, rows), Q_ARG(QString, layout));
        QCOMPARE(rows.size(), 3);
        QCOMPARE(rows.first().toMap().value(QStringLiteral("id")).toString(), QStringLiteral("claude-code"));
        QMetaObject::invokeMethod(popup, "layoutMoveBlock", Q_RETURN_ARG(QString, layout), Q_ARG(QString, layout),
                                  Q_ARG(int, 2), Q_ARG(int, 0));
        QMetaObject::invokeMethod(popup, "layoutSetShown", Q_RETURN_ARG(QString, layout), Q_ARG(QString, layout),
                                  Q_ARG(QString, QStringLiteral("heatmap")), Q_ARG(bool, false));
        QMetaObject::invokeMethod(popup, "layoutSetSectionShown", Q_RETURN_ARG(QString, layout), Q_ARG(QString, layout),
                                  Q_ARG(QString, QStringLiteral("claude-code")),
                                  Q_ARG(QString, QStringLiteral("insights")), Q_ARG(bool, false));
        QMetaObject::invokeMethod(popup, "layoutRows", Q_RETURN_ARG(QVariantList, rows), Q_ARG(QString, layout));
        QCOMPARE(rows.first().toMap().value(QStringLiteral("id")).toString(), QStringLiteral("github"));
        QVERIFY(!rows.at(2).toMap().value(QStringLiteral("shown")).toBool()); // heatmap
        const QVariantList sections = rows.at(1).toMap().value(QStringLiteral("sections")).toList();
        QVERIFY(!sections.at(2).toMap().value(QStringLiteral("shown")).toBool());
    }

    QObject *theme(QQmlEngine &engine) {
        return engine.singletonInstance<QObject *>(QString::fromLatin1(uri), QStringLiteral("Theme"));
    }

    void customTokensOverrideTheTheme() {
        QQmlEngine engine;
        QObject *popup = model(engine);
        QObject *style = theme(engine);
        QCOMPARE(style->property("accent").value<QColor>(), QColor(QStringLiteral("#0a84ff")));
        QMetaObject::invokeMethod(
            popup, "applySettings",
            Q_ARG(QVariantMap, (QVariantMap{{QStringLiteral("look"), QStringLiteral("Custom")},
                                            {QStringLiteral("themeTokens"),
                                             QStringLiteral("{\"accent\":\"#112233\",\"padding\":20}")}})));
        QCOMPARE(style->property("accent").value<QColor>(), QColor(QStringLiteral("#112233")));
        QCOMPARE(style->property("padding").toInt(), 20);
        QMetaObject::invokeMethod(popup, "applySettings", Q_ARG(QVariantMap, QVariantMap()));
        QCOMPARE(style->property("accent").value<QColor>(), QColor(QStringLiteral("#0a84ff")));
    }

    // Review Focus 4.
    void plasmaLookFollowsThePalette() {
        QQmlEngine engine;
        QObject *popup = model(engine);
        QObject *style = theme(engine);
        QMetaObject::invokeMethod(
            popup, "applySettings",
            Q_ARG(QVariantMap, (QVariantMap{{QStringLiteral("look"), QStringLiteral("Plasma")}})));
        QCOMPARE(style->property("accent").value<QColor>(), QGuiApplication::palette().color(QPalette::Highlight));
        QMetaObject::invokeMethod(popup, "applySettings", Q_ARG(QVariantMap, QVariantMap()));
    }

    // Review Focus 5.
    void densityScalesSpacing() {
        QQmlEngine engine;
        QObject *popup = model(engine);
        QObject *style = theme(engine);
        const int padding = style->property("padding").toInt();
        QMetaObject::invokeMethod(
            popup, "applySettings",
            Q_ARG(QVariantMap, (QVariantMap{{QStringLiteral("density"), QStringLiteral("compact")}})));
        QVERIFY(style->property("padding").toInt() < padding);
        QVERIFY(style->property("spacing").toInt() < 8);
        QMetaObject::invokeMethod(popup, "applySettings", Q_ARG(QVariantMap, QVariantMap()));
    }

    // A provider update can hand the section no insights for a moment: no script errors.
    void insightsWithoutDataRaisesNoErrors() {
        QTest::failOnWarning(QRegularExpression(QStringLiteral(".*TypeError.*")));
        QQmlEngine engine;
        QQmlComponent component(
            &engine, QUrl(QStringLiteral("qrc:/qt/qml/plasma/applet/org/contribusage/widget/InsightsSection.qml")));
        std::unique_ptr<QObject> section(
            component.createWithInitialProperties({{QStringLiteral("insights"), QVariant()}}));
        QVERIFY2(section, qPrintable(component.errorString()));
    }

    // Overrides live in a folder the test controls: XDG_DATA_HOME is set before the model reads it.
    QString writeOverride(const QTemporaryDir &dir, const QString &block, const QByteArray &qml) {
        const QString folder = dir.filePath(QStringLiteral("contribusage/qml"));
        QDir().mkpath(folder);
        QFile file(folder + u'/' + block + QStringLiteral(".qml"));
        if (file.open(QIODevice::WriteOnly)) {
            file.write(qml);
        }
        return file.fileName();
    }

    // Review Focus 3.
    void anOverrideReplacesTheBlock() {
        QTemporaryDir dir;
        qputenv("XDG_DATA_HOME", dir.path().toUtf8());
        writeOverride(
            dir, QStringLiteral("GitHubSection"),
            "import QtQuick\nItem { objectName: \"customGitHub\"; property var github; implicitHeight: 10 }\n");
        QQmlEngine engine;
        QQmlComponent component(
            &engine, QUrl(QStringLiteral("qrc:/qt/qml/plasma/applet/org/contribusage/widget/FullPopup.qml")));
        std::unique_ptr<QObject> popup(component.create());
        QVERIFY2(popup, qPrintable(component.errorString()));
        QTRY_VERIFY(findItem(popup.get(), QStringLiteral("customGitHub")));
        qunsetenv("XDG_DATA_HOME");
    }

    // Review Focus 2.
    void aBrokenOverrideFallsBack() {
        QTemporaryDir dir;
        qputenv("XDG_DATA_HOME", dir.path().toUtf8());
        writeOverride(dir, QStringLiteral("GitHubSection"), "import QtQuick\nItem { this is not qml\n");
        QQmlEngine engine;
        QQmlComponent component(
            &engine, QUrl(QStringLiteral("qrc:/qt/qml/plasma/applet/org/contribusage/widget/FullPopup.qml")));
        std::unique_ptr<QObject> popup(component.create());
        QVERIFY2(popup, qPrintable(component.errorString()));
        QTRY_VERIFY(findItem(popup.get(), QStringLiteral("overrideNotice")));
        QVERIFY(findItem(popup.get(), QStringLiteral("tokenField")) ||
                findItem(popup.get(), QStringLiteral("githubBuiltIn")));
        qunsetenv("XDG_DATA_HOME");
    }
};

QTEST_MAIN(TestAppletQml)
#include "tst_appletqml.moc"
