#include "core/appearance.h"

#include <QTest>

using namespace contribusage;

// Port design §3: the look, the density and the token overrides, validated.
class TestAppearance : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void defaultsAreTheMacOSLook() {
        const QVariantMap appearance = appearanceFromMap({});
        QCOMPARE(appearance.value(QStringLiteral("look")).toString(), QStringLiteral("macOS"));
        QCOMPARE(appearance.value(QStringLiteral("density")).toString(), QStringLiteral("comfortable"));
        QVERIFY(appearance.value(QStringLiteral("tokens")).toMap().isEmpty());
    }

    // Review Focus 1.
    void unknownAndWrongTokensAreDropped() {
        const QVariantMap tokens = themeTokensFromText(QStringLiteral(
            R"({"accent":"#112233","claudeHue":"#80ff0000","padding":20,"popupWidth":5000,"spacing":"big","warning":12,"secondaryOpacity":0.5,"nonsense":1,"critical":"red"})"));
        QCOMPARE(tokens.value(QStringLiteral("accent")).toString(), QStringLiteral("#112233"));
        QCOMPARE(tokens.value(QStringLiteral("claudeHue")).toString(), QStringLiteral("#80ff0000"));
        QCOMPARE(tokens.value(QStringLiteral("padding")).toInt(), 20);
        QCOMPARE(tokens.value(QStringLiteral("popupWidth")).toInt(), 640);
        QCOMPARE(tokens.value(QStringLiteral("secondaryOpacity")).toDouble(), 0.5);
        QVERIFY(!tokens.contains(QStringLiteral("spacing")));
        QVERIFY(!tokens.contains(QStringLiteral("warning")));
        QVERIFY(!tokens.contains(QStringLiteral("nonsense")));
        QVERIFY(!tokens.contains(QStringLiteral("critical")));
        QVERIFY(themeTokensFromText(QStringLiteral("{broken")).isEmpty());
    }

    void tokensApplyOnlyToTheCustomLook() {
        const QString tokens = QStringLiteral(R"({"accent":"#112233"})");
        QVERIFY(appearanceFromMap(
                    {{QStringLiteral("look"), QStringLiteral("Plasma")}, {QStringLiteral("themeTokens"), tokens}})
                    .value(QStringLiteral("tokens"))
                    .toMap()
                    .isEmpty());
        QCOMPARE(appearanceFromMap(
                     {{QStringLiteral("look"), QStringLiteral("Custom")}, {QStringLiteral("themeTokens"), tokens}})
                     .value(QStringLiteral("tokens"))
                     .toMap()
                     .value(QStringLiteral("accent"))
                     .toString(),
                 QStringLiteral("#112233"));
        QCOMPARE(appearanceFromMap({{QStringLiteral("look"), QStringLiteral("odd")}})
                     .value(QStringLiteral("look"))
                     .toString(),
                 QStringLiteral("macOS"));
        QCOMPARE(appearanceFromMap({{QStringLiteral("density"), QStringLiteral("compact")}})
                     .value(QStringLiteral("density"))
                     .toString(),
                 QStringLiteral("compact"));
    }
};

QTEST_GUILESS_MAIN(TestAppearance)
#include "tst_appearance.moc"
