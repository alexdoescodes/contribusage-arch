#include <QFile>
#include <QPluginLoader>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QTest>

// Loads the built applet and compiles its QML the way plasmashell would: a QML error or a missing type fails here,
// not as an empty widget in the panel.
class TestAppletQml : public QObject {
    Q_OBJECT

    static constexpr auto uri = "plasma.applet.org.contribusage.widget";
    QPluginLoader m_loader{QStringLiteral(CONTRIBUSAGE_APPLET_PATH)};

private Q_SLOTS:
    void initTestCase() { QVERIFY2(m_loader.load(), qPrintable(m_loader.errorString())); }

    void mainQmlCompiles() {
        QQmlEngine engine;
        QQmlComponent component(&engine,
                                QUrl(QStringLiteral("qrc:/qt/qml/plasma/applet/org/contribusage/widget/main.qml")));
        QVERIFY2(component.status() == QQmlComponent::Ready, qPrintable(component.errorString()));
    }

    void appInfoSingletonReportsTheVersion() {
        QQmlEngine engine;
        auto *appInfo = engine.singletonInstance<QObject *>(QString::fromLatin1(uri), QStringLiteral("AppInfo"));
        QVERIFY(appInfo);
        QCOMPARE(appInfo->property("version").toString(), QStringLiteral("0.1.0"));
    }
};

QTEST_MAIN(TestAppletQml)
#include "tst_appletqml.moc"
