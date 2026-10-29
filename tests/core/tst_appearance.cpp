#include "core/appearance.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QTest>

using namespace contribusage;

// Writes `text` to `folder/name`, creating the folders on the way.
static void writeFile(const QString &folder, const QString &name, const QByteArray &text) {
    QDir().mkpath(QFileInfo(folder + u'/' + name).absolutePath());
    QFile file(folder + u'/' + name);
    QVERIFY(file.open(QIODevice::WriteOnly));
    QCOMPARE(file.write(text), text.size());
}

// Port design §3, ADR-050: the theme, the density and the token overrides, validated; the installed Plasma styles.
class TestAppearance : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void defaultsFollowTheSystem() {
        const QVariantMap appearance = appearanceFromMap({});
        QCOMPARE(appearance.value(QStringLiteral("theme")).toString(), QStringLiteral("system"));
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

    void tokensApplyToEveryTheme() {
        const QString tokens = QStringLiteral(R"({"accent":"#112233"})");
        for (const auto *theme : {"system", "macOS"}) {
            QCOMPARE(appearanceFromMap({{QStringLiteral("theme"), QString::fromLatin1(theme)},
                                        {QStringLiteral("themeTokens"), tokens}})
                         .value(QStringLiteral("tokens"))
                         .toMap()
                         .value(QStringLiteral("accent"))
                         .toString(),
                     QStringLiteral("#112233"));
        }
        QCOMPARE(appearanceFromMap({{QStringLiteral("density"), QStringLiteral("compact")}})
                     .value(QStringLiteral("density"))
                     .toString(),
                 QStringLiteral("compact"));
    }

    // ADR-050: the old `look` setting picks the theme until a theme is chosen.
    void theOldLookPicksTheTheme() {
        const auto theme = [](const QVariantMap &configuration) {
            return appearanceFromMap(configuration).value(QStringLiteral("theme")).toString();
        };
        QCOMPARE(theme({{QStringLiteral("look"), QStringLiteral("macOS")}}), QStringLiteral("macOS"));
        QCOMPARE(theme({{QStringLiteral("look"), QStringLiteral("Custom")}}), QStringLiteral("macOS"));
        QCOMPARE(theme({{QStringLiteral("look"), QStringLiteral("Plasma")}}), QStringLiteral("system"));
        QCOMPARE(theme({{QStringLiteral("look"), QStringLiteral("odd")}}), QStringLiteral("system"));
        QCOMPARE(theme({{QStringLiteral("look"), QStringLiteral("macOS")},
                        {QStringLiteral("theme"), QStringLiteral("system")}}),
                 QStringLiteral("system"));
        QCOMPARE(theme({{QStringLiteral("theme"), QStringLiteral("odd")}}), QStringLiteral("system"));
    }

    // ADR-050: a style counts when its folder has metadata; the first data folder wins, as in Plasma.
    void plasmaStylesAreFoundInTheDataFolders() {
        QTemporaryDir home;
        QTemporaryDir system;
        writeFile(home.path(), QStringLiteral("plasma/desktoptheme/one/metadata.json"),
                  R"({"KPlugin": {"Id": "one", "Name": "Zed One"}})");
        writeFile(home.path(), QStringLiteral("plasma/desktoptheme/two/metadata.desktop"),
                  "[Desktop Entry]\nName=Alpha Two\nComment=x\n");
        writeFile(home.path(), QStringLiteral("plasma/desktoptheme/clone/README.md"), "not a style");
        writeFile(system.path(), QStringLiteral("plasma/desktoptheme/one/metadata.json"),
                  R"({"KPlugin": {"Id": "one", "Name": "Shadowed"}})");
        writeFile(system.path(), QStringLiteral("plasma/desktoptheme/bare/metadata.json"), "{}");
        const QVariantList styles = plasmaStyles({home.path(), system.path()});
        QCOMPARE(styles.size(), 3);
        // Sorted by name; a style without a name shows its folder name.
        QCOMPARE(styles.at(0).toMap().value(QStringLiteral("id")).toString(), QStringLiteral("two"));
        QCOMPARE(styles.at(0).toMap().value(QStringLiteral("name")).toString(), QStringLiteral("Alpha Two"));
        QCOMPARE(styles.at(1).toMap().value(QStringLiteral("name")).toString(), QStringLiteral("bare"));
        QCOMPARE(styles.at(2).toMap().value(QStringLiteral("name")).toString(), QStringLiteral("Zed One"));
    }

    // A style without its own image uses the one of Plasma's `default` style, as Plasma does.
    void styleFilesFallBackToTheDefaultStyle() {
        QTemporaryDir home;
        QTemporaryDir system;
        writeFile(home.path(), QStringLiteral("plasma/desktoptheme/one/metadata.json"), "{}");
        writeFile(home.path(), QStringLiteral("plasma/desktoptheme/one/dialogs/background.svgz"), "svgz");
        writeFile(system.path(), QStringLiteral("plasma/desktoptheme/default/metadata.json"), "{}");
        writeFile(system.path(), QStringLiteral("plasma/desktoptheme/default/widgets/bar_meter_horizontal.svg"), "svg");
        const QStringList dirs{home.path(), system.path()};
        QCOMPARE(plasmaStyleFile(dirs, QStringLiteral("one"), QStringLiteral("dialogs/background")),
                 home.filePath(QStringLiteral("plasma/desktoptheme/one/dialogs/background.svgz")));
        QCOMPARE(plasmaStyleFile(dirs, QStringLiteral("one"), QStringLiteral("widgets/bar_meter_horizontal")),
                 system.filePath(QStringLiteral("plasma/desktoptheme/default/widgets/bar_meter_horizontal.svg")));
        QVERIFY(plasmaStyleFile(dirs, QStringLiteral("one"), QStringLiteral("widgets/missing")).isEmpty());
        QVERIFY(plasmaStyleFile(dirs, QStringLiteral("../one"), QStringLiteral("dialogs/background")).isEmpty());
    }

    // A style's `colors` file is a KDE color scheme; the popup reads the window and selection colors.
    void colorSchemeIsRead() {
        const QVariantMap colors = colorSchemeFromText(
            QStringLiteral("[Colors:View]\nForegroundNormal=1,1,1\n"
                           "[Colors:Window]\nBackgroundNormal=54,54,54\nForegroundNormal=255,255,255\n"
                           "ForegroundNegative=237,21,21\nForegroundNeutral=246,116,0,255\nForegroundPositive=bad\n"
                           "[Colors:Selection]\nBackgroundNormal=49,91,239\n"));
        QCOMPARE(colors.value(QStringLiteral("textColor")).toString(), QStringLiteral("#ffffff"));
        QCOMPARE(colors.value(QStringLiteral("backgroundColor")).toString(), QStringLiteral("#363636"));
        QCOMPARE(colors.value(QStringLiteral("highlightColor")).toString(), QStringLiteral("#315bef"));
        QCOMPARE(colors.value(QStringLiteral("negativeTextColor")).toString(), QStringLiteral("#ed1515"));
        QCOMPARE(colors.value(QStringLiteral("neutralTextColor")).toString(), QStringLiteral("#f67400"));
        QVERIFY(!colors.contains(QStringLiteral("positiveTextColor")));
        QVERIFY(colorSchemeFromText(QStringLiteral("garbage")).isEmpty());
    }

    // ADR-050: a style theme carries its background, its meter image and its colors; an unknown one is the system's.
    void aStyleThemeCarriesItsFiles() {
        QTemporaryDir home;
        writeFile(home.path(), QStringLiteral("plasma/desktoptheme/one/metadata.json"), "{}");
        writeFile(home.path(), QStringLiteral("plasma/desktoptheme/one/dialogs/background.svg"), "svg");
        writeFile(home.path(), QStringLiteral("plasma/desktoptheme/one/colors"),
                  "[Colors:Window]\nForegroundNormal=10,20,30\n");
        const QVariantMap appearance =
            appearanceFromMap({{QStringLiteral("theme"), QStringLiteral("plasma:one")}}, {home.path()});
        QCOMPARE(appearance.value(QStringLiteral("theme")).toString(), QStringLiteral("plasma:one"));
        QCOMPARE(appearance.value(QStringLiteral("background")).toString(),
                 home.filePath(QStringLiteral("plasma/desktoptheme/one/dialogs/background.svg")));
        QCOMPARE(appearance.value(QStringLiteral("colors")).toMap().value(QStringLiteral("textColor")).toString(),
                 QStringLiteral("#0a141e"));
        QCOMPARE(appearanceFromMap({{QStringLiteral("theme"), QStringLiteral("plasma:gone")}}, {home.path()})
                     .value(QStringLiteral("theme"))
                     .toString(),
                 QStringLiteral("system"));
        // The system theme draws Plasma's own meter; macOS draws its capsule.
        QCOMPARE(appearanceFromMap({}).value(QStringLiteral("meter")).toString(),
                 QStringLiteral("widgets/bar_meter_horizontal"));
        QVERIFY(appearanceFromMap({{QStringLiteral("theme"), QStringLiteral("macOS")}})
                    .value(QStringLiteral("meter"))
                    .toString()
                    .isEmpty());
    }

    // ADR-050: a global theme (look-and-feel package) lists by its name; a package without defaults is no theme.
    void globalThemesListWithTheirDefaults() {
        QTemporaryDir home;
        const QString lnf = QStringLiteral("plasma/look-and-feel/");
        writeFile(home.path(), lnf + QStringLiteral("org.example.dark/metadata.json"),
                  R"({"KPlugin":{"Name":"Example Dark"}})");
        writeFile(home.path(), lnf + QStringLiteral("org.example.dark/contents/defaults"),
                  "[kdeglobals][General]\nColorScheme=ExampleDark\n\n[kdeglobals][Icons]\nTheme=Example-dark\n\n"
                  "[kdeglobals][KDE]\nwidgetStyle=kvantum\n\n[plasmarc][Theme]\nname=example-style\n");
        writeFile(home.path(), lnf + QStringLiteral("org.example.splash/metadata.json"),
                  R"({"KPlugin":{"Name":"Splash only"}})");
        const QVariantList themes = globalThemes({home.path()});
        QCOMPARE(themes.size(), 1);
        QCOMPARE(themes.first().toMap().value(QStringLiteral("id")).toString(), QStringLiteral("org.example.dark"));
        QCOMPARE(themes.first().toMap().value(QStringLiteral("name")).toString(), QStringLiteral("Example Dark"));
        const QVariantMap defaults = globalThemeDefaults({home.path()}, QStringLiteral("org.example.dark"));
        QCOMPARE(defaults.value(QStringLiteral("style")).toString(), QStringLiteral("example-style"));
        QCOMPARE(defaults.value(QStringLiteral("colorScheme")).toString(), QStringLiteral("ExampleDark"));
        QCOMPARE(defaults.value(QStringLiteral("iconTheme")).toString(), QStringLiteral("Example-dark"));
        QVERIFY(globalThemeDefaults({home.path()}, QStringLiteral("org.example.splash")).isEmpty());
        QVERIFY(globalThemeDefaults({home.path()}, QStringLiteral("../x")).isEmpty());
    }

    // One pick sets the style, the color scheme and the icon theme of the widget.
    void aGlobalThemeCarriesStyleColorsAndIcons() {
        QTemporaryDir home;
        writeFile(home.path(), QStringLiteral("plasma/look-and-feel/org.example.dark/metadata.json"), "{}");
        writeFile(home.path(), QStringLiteral("plasma/look-and-feel/org.example.dark/contents/defaults"),
                  "[kdeglobals][General]\nColorScheme=ExampleDark\n[kdeglobals][Icons]\nTheme=Example-dark\n"
                  "[plasmarc][Theme]\nname=example-style\n");
        writeFile(home.path(), QStringLiteral("plasma/desktoptheme/example-style/metadata.json"), "{}");
        writeFile(home.path(), QStringLiteral("plasma/desktoptheme/example-style/dialogs/background.svg"), "svg");
        writeFile(home.path(), QStringLiteral("color-schemes/ExampleDark.colors"),
                  "[Colors:Window]\nForegroundNormal=1,2,3\n");
        const QVariantMap appearance =
            appearanceFromMap({{QStringLiteral("theme"), QStringLiteral("global:org.example.dark")}}, {home.path()});
        QCOMPARE(appearance.value(QStringLiteral("theme")).toString(), QStringLiteral("global:org.example.dark"));
        QCOMPARE(appearance.value(QStringLiteral("style")).toString(), QStringLiteral("example-style"));
        QCOMPARE(appearance.value(QStringLiteral("background")).toString(),
                 home.filePath(QStringLiteral("plasma/desktoptheme/example-style/dialogs/background.svg")));
        QCOMPARE(appearance.value(QStringLiteral("colors")).toMap().value(QStringLiteral("textColor")).toString(),
                 QStringLiteral("#010203"));
        QCOMPARE(appearance.value(QStringLiteral("iconTheme")).toString(), QStringLiteral("Example-dark"));
        QCOMPARE(appearanceFromMap({{QStringLiteral("theme"), QStringLiteral("global:gone")}}, {home.path()})
                     .value(QStringLiteral("theme"))
                     .toString(),
                 QStringLiteral("system"));
    }

    // A global theme whose style is Plasma's `default` leaves the popup's frame to the system, with its colors and
    // icons.
    void aGlobalThemeOnTheDefaultStyleKeepsTheSystemFrame() {
        QTemporaryDir home;
        writeFile(home.path(), QStringLiteral("plasma/look-and-feel/org.example.breeze/metadata.json"), "{}");
        writeFile(home.path(), QStringLiteral("plasma/look-and-feel/org.example.breeze/contents/defaults"),
                  "[kdeglobals][Icons]\nTheme=breeze\n[plasmarc][Theme]\nname=default\n");
        const QVariantMap appearance =
            appearanceFromMap({{QStringLiteral("theme"), QStringLiteral("global:org.example.breeze")}}, {home.path()});
        QCOMPARE(appearance.value(QStringLiteral("theme")).toString(), QStringLiteral("global:org.example.breeze"));
        QVERIFY(appearance.value(QStringLiteral("style")).toString().isEmpty());
        QCOMPARE(appearance.value(QStringLiteral("iconTheme")).toString(), QStringLiteral("breeze"));
    }

    // ADR-050: icons come from the chosen theme's folders (nearest size, then the inherited themes), or not at all.
    void iconsResolveThroughTheIconThemeAndItsParents() {
        QTemporaryDir home;
        QTemporaryDir system;
        writeFile(home.path(), QStringLiteral("icons/Child/index.theme"),
                  "[Icon Theme]\nName=Child\nInherits=Parent\nDirectories=actions/16,actions/48,actions/symbolic\n\n"
                  "[actions/16]\nSize=16\nType=Fixed\n[actions/48]\nSize=48\nType=Fixed\n"
                  "[actions/symbolic]\nSize=16\nMinSize=16\nMaxSize=512\nType=Scalable\n");
        writeFile(home.path(), QStringLiteral("icons/Child/actions/16/configure.svg"), "small");
        writeFile(home.path(), QStringLiteral("icons/Child/actions/48/configure.svg"), "large");
        writeFile(home.path(), QStringLiteral("icons/Child/actions/symbolic/draw-star.svg"), "scalable");
        writeFile(system.path(), QStringLiteral("icons/Parent/index.theme"),
                  "[Icon Theme]\nName=Parent\nDirectories=actions/22\n\n[actions/22]\nSize=22\nType=Fixed\n");
        writeFile(system.path(), QStringLiteral("icons/Parent/actions/22/edit-copy.svg"), "inherited");
        const QStringList dirs{home.path(), system.path()};
        QCOMPARE(iconFile(dirs, QStringLiteral("Child"), QStringLiteral("configure")),
                 home.filePath(QStringLiteral("icons/Child/actions/16/configure.svg")));
        QCOMPARE(iconFile(dirs, QStringLiteral("Child"), QStringLiteral("draw-star")),
                 home.filePath(QStringLiteral("icons/Child/actions/symbolic/draw-star.svg")));
        QCOMPARE(iconFile(dirs, QStringLiteral("Child"), QStringLiteral("edit-copy")),
                 system.filePath(QStringLiteral("icons/Parent/actions/22/edit-copy.svg")));
        QVERIFY(iconFile(dirs, QStringLiteral("Child"), QStringLiteral("missing")).isEmpty());
        QVERIFY(iconFile(dirs, QStringLiteral("Gone"), QStringLiteral("configure")).isEmpty());
        QVERIFY(iconFile(dirs, QStringLiteral("../Child"), QStringLiteral("configure")).isEmpty());
        QVERIFY(iconFile(dirs, QStringLiteral("Child"), QStringLiteral("../configure")).isEmpty());
    }

    // Two themes inheriting each other end the search instead of looping.
    void inheritingCyclesTerminate() {
        QTemporaryDir home;
        writeFile(home.path(), QStringLiteral("icons/A/index.theme"), "[Icon Theme]\nInherits=B\nDirectories=\n");
        writeFile(home.path(), QStringLiteral("icons/B/index.theme"), "[Icon Theme]\nInherits=A\nDirectories=\n");
        QVERIFY(iconFile({home.path()}, QStringLiteral("A"), QStringLiteral("configure")).isEmpty());
    }

    // The macOS theme takes the WhiteSur icon theme of the system's brightness when installed.
    void macOSAsksForTheWhiteSurIcons() {
        QCOMPARE(appearanceFromMap({{QStringLiteral("theme"), QStringLiteral("macOS")}})
                     .value(QStringLiteral("iconTheme"))
                     .toString(),
                 QStringLiteral("WhiteSur"));
        QVERIFY(appearanceFromMap({}).value(QStringLiteral("iconTheme")).toString().isEmpty());
    }
};

QTEST_GUILESS_MAIN(TestAppearance)
#include "tst_appearance.moc"
