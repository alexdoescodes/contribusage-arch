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
    void initTestCase() { QVERIFY2(m_loader.load(), qPrintable(m_loader.errorString())); }

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
        QCOMPARE(windows.size(), 3);
        const auto session = windows.at(0).toMap();
        QCOMPARE(session.value(QStringLiteral("percentText")).toString(), QStringLiteral("23%"));
        QCOMPARE(session.value(QStringLiteral("level")).toString(), QStringLiteral("normal"));
        QCOMPARE(session.value(QStringLiteral("resetText")).toString(), QStringLiteral("resets in 2 hr 10 min"));
        QCOMPARE(windows.at(1).toMap().value(QStringLiteral("level")).toString(), QStringLiteral("warning"));
        QCOMPARE(windows.at(2).toMap().value(QStringLiteral("level")).toString(), QStringLiteral("critical"));
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
};

QTEST_MAIN(TestAppletQml)
#include "tst_appletqml.moc"
