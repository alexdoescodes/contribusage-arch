#include "core/display.h"
#include "core/heatmap.h"

#include <QFile>
#include <QPluginLoader>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickWindow>
#include <QTest>

#include <functional>
#include <memory>

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
        for (const auto *name :
             {"main", "Theme", "SectionHeader", "LimitBar", "ActivitySection", "InsightsSection", "ProviderGroup",
              "HeatmapBlock", "GitHubSection", "Footer", "FullPopup", "PromptGlyph", "CompactLabel"}) {
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
};

QTEST_MAIN(TestAppletQml)
#include "tst_appletqml.moc"
