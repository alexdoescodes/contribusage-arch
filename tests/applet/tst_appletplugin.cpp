#include <KPluginMetaData>
#include <QCoreApplication>
#include <QTest>

// Finds the built applet the way Plasma does: by id, under plasma/applets in the plugin paths.
class TestAppletPlugin : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void initTestCase() { QCoreApplication::addLibraryPath(QStringLiteral(CONTRIBUSAGE_PLUGIN_DIR)); }

    void appletIsFoundById() {
        const auto metaData = KPluginMetaData::findPluginById(QStringLiteral("plasma/applets"),
                                                              QStringLiteral("org.contribusage.widget"));
        QVERIFY2(metaData.isValid(), "org.contribusage.widget not found under plasma/applets");
        QCOMPARE(metaData.name(), QStringLiteral("contribusage"));
        QCOMPARE(metaData.version(), QStringLiteral("0.1.0"));
        QCOMPARE(metaData.value(QStringLiteral("X-Plasma-API-Minimum-Version")), QStringLiteral("6.0"));
    }
};

QTEST_GUILESS_MAIN(TestAppletPlugin)
#include "tst_appletplugin.moc"
