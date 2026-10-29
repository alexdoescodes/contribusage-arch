#include "core/appearance.h"

#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>

#include <algorithm>
#include <cmath>
#include <limits>

namespace contribusage {

namespace {

struct Range {
    double low;
    double high;
};

QString styleFolder(const QString &dataDir, const QString &style) {
    return dataDir + QStringLiteral("/plasma/desktoptheme/") + style;
}

QByteArray readFile(const QString &path) {
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
}

// A style's shown name: KPlugin.Name from metadata.json, else Name= from metadata.desktop, else the folder name.
QString styleName(const QString &folder, const QString &id) {
    const QJsonObject plugin = QJsonDocument::fromJson(readFile(folder + QStringLiteral("/metadata.json")))
                                   .object()
                                   .value(u"KPlugin")
                                   .toObject();
    if (const QString name = plugin.value(u"Name").toString(); !name.isEmpty()) {
        return name;
    }
    const QString desktop = QString::fromUtf8(readFile(folder + QStringLiteral("/metadata.desktop")));
    static const QRegularExpression nameLine(QStringLiteral("^Name=(.+)$"), QRegularExpression::MultilineOption);
    if (const auto match = nameLine.match(desktop); match.hasMatch()) {
        return match.captured(1).trimmed();
    }
    return id;
}

QString lookAndFeelFolder(const QString &dataDir, const QString &id) {
    return dataDir + QStringLiteral("/plasma/look-and-feel/") + id;
}

// The key=value lines of a desktop-style file by section: "[a][b]" or "a/16" stay as written without the outer brackets
// of a single-bracket header.
QMap<QString, QMap<QString, QString>> iniSections(const QByteArray &bytes) {
    QMap<QString, QMap<QString, QString>> sections;
    QString section;
    for (const QString &raw : QString::fromUtf8(bytes).split(u'\n')) {
        const QString line = raw.trimmed();
        if (line.startsWith(u'[') && line.endsWith(u']')) {
            section = line.mid(1, line.size() - 2);
            continue;
        }
        const qsizetype equals = line.indexOf(u'=');
        if (equals > 0 && !line.startsWith(u'#')) {
            sections[section].insert(line.left(equals).trimmed(), line.mid(equals + 1).trimmed());
        }
    }
    return sections;
}

// How far an icon directory is from the wanted size (0: it serves the size exactly).
int sizeDistance(const QMap<QString, QString> &directory, int wanted) {
    const int size = directory.value(QStringLiteral("Size")).toInt();
    const QString type = directory.value(QStringLiteral("Type"), QStringLiteral("Threshold"));
    if (type == QStringLiteral("Scalable")) {
        const int low = directory.value(QStringLiteral("MinSize"), QString::number(size)).toInt();
        const int high = directory.value(QStringLiteral("MaxSize"), QString::number(size)).toInt();
        return wanted < low ? low - wanted : wanted > high ? wanted - high : 0;
    }
    if (type == QStringLiteral("Fixed")) {
        return std::abs(size - wanted);
    }
    const int threshold = directory.value(QStringLiteral("Threshold"), QStringLiteral("2")).toInt();
    return std::max(0, std::abs(size - wanted) - threshold);
}

bool isGlobalTheme(const QString &folder) {
    return QFile::exists(folder + QStringLiteral("/contents/defaults"));
}

bool isStyle(const QString &folder) {
    return QFile::exists(folder + QStringLiteral("/metadata.json")) ||
           QFile::exists(folder + QStringLiteral("/metadata.desktop"));
}

bool validStyleId(const QString &id) {
    return !id.isEmpty() && !id.contains(u'/') && id != QStringLiteral("..") && id != QStringLiteral(".");
}

} // namespace

QVariantMap themeTokensFromText(const QString &json) {
    static const QRegularExpression color(QStringLiteral("^#([0-9a-fA-F]{6}|[0-9a-fA-F]{8})$"));
    static const QStringList colors{QStringLiteral("accent"), QStringLiteral("warning"), QStringLiteral("critical"),
                                    QStringLiteral("claudeHue"), QStringLiteral("githubHue")};
    static const QMap<QString, Range> numbers{
        {QStringLiteral("secondaryOpacity"), {0, 1}}, {QStringLiteral("trackOpacity"), {0, 1}},
        {QStringLiteral("emptyCellOpacity"), {0, 1}}, {QStringLiteral("dividerOpacity"), {0, 1}},
        {QStringLiteral("mutedBarOpacity"), {0, 1}},  {QStringLiteral("popupWidth"), {280, 640}},
        {QStringLiteral("padding"), {0, 64}},         {QStringLiteral("spacing"), {0, 64}},
        {QStringLiteral("meterHeight"), {0, 64}},     {QStringLiteral("cellGap"), {0, 64}},
        {QStringLiteral("weekChartHeight"), {0, 64}}};
    const QJsonDocument document = QJsonDocument::fromJson(json.toUtf8());
    QVariantMap tokens;
    if (!document.isObject()) {
        return tokens;
    }
    const QJsonObject object = document.object();
    for (auto it = object.begin(); it != object.end(); ++it) {
        if (colors.contains(it.key())) {
            if (it.value().isString() && color.match(it.value().toString()).hasMatch()) {
                tokens.insert(it.key(), it.value().toString());
            }
        } else if (const auto range = numbers.constFind(it.key()); range != numbers.cend() && it.value().isDouble()) {
            tokens.insert(it.key(), std::clamp(it.value().toDouble(), range->low, range->high));
        }
    }
    return tokens;
}

QVariantList plasmaStyles(const QStringList &dataDirs) {
    QStringList seen;
    QList<QVariantMap> styles;
    for (const QString &dataDir : dataDirs) {
        const QDir root(dataDir + QStringLiteral("/plasma/desktoptheme"));
        for (const QString &id : root.entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
            const QString folder = root.filePath(id);
            if (seen.contains(id) || !isStyle(folder)) {
                continue;
            }
            seen.append(id);
            styles.append({{QStringLiteral("id"), id}, {QStringLiteral("name"), styleName(folder, id)}});
        }
    }
    std::ranges::sort(styles, [](const QVariantMap &a, const QVariantMap &b) {
        return a.value(QStringLiteral("name"))
                   .toString()
                   .compare(b.value(QStringLiteral("name")).toString(), Qt::CaseInsensitive) < 0;
    });
    QVariantList list;
    for (const QVariantMap &style : styles) {
        list.append(style);
    }
    return list;
}

QString plasmaStyleFile(const QStringList &dataDirs, const QString &style, const QString &image) {
    if (!validStyleId(style)) {
        return {};
    }
    for (const QString &id : {style, QStringLiteral("default")}) {
        for (const QString &dataDir : dataDirs) {
            const QString folder = styleFolder(dataDir, id);
            if (!isStyle(folder)) {
                continue;
            }
            for (const auto *suffix : {".svgz", ".svg"}) {
                const QString file = folder + u'/' + image + QLatin1String(suffix);
                if (QFile::exists(file)) {
                    return file;
                }
            }
            break; // the first data folder holding the style is the style
        }
    }
    return {};
}

QVariantMap colorSchemeFromText(const QString &text) {
    static const QMap<QString, QString> window{
        {QStringLiteral("ForegroundNormal"), QStringLiteral("textColor")},
        {QStringLiteral("BackgroundNormal"), QStringLiteral("backgroundColor")},
        {QStringLiteral("ForegroundNegative"), QStringLiteral("negativeTextColor")},
        {QStringLiteral("ForegroundNeutral"), QStringLiteral("neutralTextColor")},
        {QStringLiteral("ForegroundPositive"), QStringLiteral("positiveTextColor")}};
    static const QRegularExpression rgb(QStringLiteral("^(\\d{1,3}),(\\d{1,3}),(\\d{1,3})(,\\d{1,3})?$"));
    QVariantMap colors;
    QString section;
    for (const QString &raw : text.split(u'\n')) {
        const QString line = raw.trimmed();
        if (line.startsWith(u'[')) {
            section = line;
            continue;
        }
        const qsizetype equals = line.indexOf(u'=');
        if (equals < 0) {
            continue;
        }
        const QString key = line.left(equals).trimmed();
        QString name;
        if (section == QStringLiteral("[Colors:Window]")) {
            name = window.value(key);
        } else if (section == QStringLiteral("[Colors:Selection]") && key == QStringLiteral("BackgroundNormal")) {
            name = QStringLiteral("highlightColor");
        }
        const auto match = rgb.match(line.mid(equals + 1).trimmed());
        if (name.isEmpty() || !match.hasMatch()) {
            continue;
        }
        const int red = match.captured(1).toInt();
        const int green = match.captured(2).toInt();
        const int blue = match.captured(3).toInt();
        if (red > 255 || green > 255 || blue > 255) {
            continue;
        }
        colors.insert(name, QStringLiteral("#%1%2%3")
                                .arg(red, 2, 16, QLatin1Char('0'))
                                .arg(green, 2, 16, QLatin1Char('0'))
                                .arg(blue, 2, 16, QLatin1Char('0')));
    }
    return colors;
}

QVariantList globalThemes(const QStringList &dataDirs) {
    QStringList seen;
    QList<QVariantMap> themes;
    for (const QString &dataDir : dataDirs) {
        const QDir root(dataDir + QStringLiteral("/plasma/look-and-feel"));
        for (const QString &id : root.entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
            const QString folder = root.filePath(id);
            if (seen.contains(id) || !isGlobalTheme(folder)) {
                continue;
            }
            seen.append(id);
            themes.append({{QStringLiteral("id"), id}, {QStringLiteral("name"), styleName(folder, id)}});
        }
    }
    std::ranges::sort(themes, [](const QVariantMap &a, const QVariantMap &b) {
        return a.value(QStringLiteral("name"))
                   .toString()
                   .compare(b.value(QStringLiteral("name")).toString(), Qt::CaseInsensitive) < 0;
    });
    QVariantList list;
    for (const QVariantMap &theme : themes) {
        list.append(theme);
    }
    return list;
}

QVariantMap globalThemeDefaults(const QStringList &dataDirs, const QString &id) {
    if (!validStyleId(id)) {
        return {};
    }
    for (const QString &dataDir : dataDirs) {
        const QString folder = lookAndFeelFolder(dataDir, id);
        if (!isGlobalTheme(folder)) {
            continue;
        }
        const auto sections = iniSections(readFile(folder + QStringLiteral("/contents/defaults")));
        return {
            {QStringLiteral("style"), sections.value(QStringLiteral("plasmarc][Theme")).value(QStringLiteral("name"))},
            {QStringLiteral("colorScheme"),
             sections.value(QStringLiteral("kdeglobals][General")).value(QStringLiteral("ColorScheme"))},
            {QStringLiteral("iconTheme"),
             sections.value(QStringLiteral("kdeglobals][Icons")).value(QStringLiteral("Theme"))}};
    }
    return {};
}

QString iconFile(const QStringList &dataDirs, const QString &iconTheme, const QString &name) {
    constexpr int wanted = 16;
    constexpr int maxThemes = 8;
    if (!validStyleId(iconTheme) || !validStyleId(name)) {
        return {};
    }
    QStringList queue{iconTheme};
    QStringList seen;
    while (!queue.isEmpty() && seen.size() < maxThemes) {
        const QString theme = queue.takeFirst();
        if (seen.contains(theme) || !validStyleId(theme)) {
            continue;
        }
        seen.append(theme);
        for (const QString &dataDir : dataDirs) {
            const QString folder = dataDir + QStringLiteral("/icons/") + theme;
            if (!QFile::exists(folder + QStringLiteral("/index.theme"))) {
                continue;
            }
            const auto sections = iniSections(readFile(folder + QStringLiteral("/index.theme")));
            const auto header = sections.value(QStringLiteral("Icon Theme"));
            QString best;
            int bestDistance = std::numeric_limits<int>::max();
            for (const QString &directory :
                 header.value(QStringLiteral("Directories")).split(u',', Qt::SkipEmptyParts)) {
                if (directory.contains(QStringLiteral(".."))) {
                    continue;
                }
                const int distance = sizeDistance(sections.value(directory), wanted);
                if (distance >= bestDistance) {
                    continue;
                }
                for (const auto *suffix : {".svg", ".png"}) {
                    const QString file = folder + u'/' + directory + u'/' + name + QLatin1String(suffix);
                    if (QFile::exists(file)) {
                        best = file;
                        bestDistance = distance;
                        break;
                    }
                }
            }
            if (!best.isEmpty()) {
                return best;
            }
            queue.append(header.value(QStringLiteral("Inherits")).split(u',', Qt::SkipEmptyParts));
            break; // the first data folder holding the theme is the theme
        }
    }
    return {};
}

QVariantMap appearanceFromMap(const QVariantMap &configuration, const QStringList &dataDirs) {
    static const QString system = QStringLiteral("system");
    static const QString macOS = QStringLiteral("macOS");
    static const QString stylePrefix = QStringLiteral("plasma:");
    static const QString globalPrefix = QStringLiteral("global:");
    QString theme = configuration.value(QStringLiteral("theme")).toString();
    if (theme.isEmpty()) {
        const QString look = configuration.value(QStringLiteral("look")).toString();
        theme = look == macOS || look == QStringLiteral("Custom") ? macOS : system;
    }
    QString style;
    QString colorScheme;
    QString iconTheme;
    if (theme.startsWith(globalPrefix)) {
        const QVariantMap defaults = globalThemeDefaults(dataDirs, theme.mid(globalPrefix.size()));
        if (defaults.isEmpty()) {
            theme = system;
        } else {
            style = defaults.value(QStringLiteral("style")).toString();
            colorScheme = defaults.value(QStringLiteral("colorScheme")).toString();
            iconTheme = defaults.value(QStringLiteral("iconTheme")).toString();
        }
    } else if (theme.startsWith(stylePrefix)) {
        style = theme.mid(stylePrefix.size());
    } else if (theme != macOS) {
        theme = system;
    }
    const auto installedStyle = [&](const QString &id) {
        return validStyleId(id) &&
               std::ranges::any_of(dataDirs, [&](const QString &dataDir) { return isStyle(styleFolder(dataDir, id)); });
    };
    if (!installedStyle(style)) {
        if (theme.startsWith(stylePrefix)) {
            theme = system;
        }
        style.clear();
    }
    if (style == QStringLiteral("default")) {
        style.clear(); // the system's own style: the popup frame stays Plasma's
    }
    if (theme == macOS) {
        iconTheme = QStringLiteral("WhiteSur");
    }
    QString meter;
    QString background;
    QVariantMap colors;
    if (theme == system || (theme.startsWith(globalPrefix) && style.isEmpty())) {
        meter = QStringLiteral("widgets/bar_meter_horizontal");
    } else if (!style.isEmpty()) {
        meter = plasmaStyleFile(dataDirs, style, QStringLiteral("widgets/bar_meter_horizontal"));
        background = plasmaStyleFile(dataDirs, style, QStringLiteral("dialogs/background"));
        for (const QString &dataDir : dataDirs) {
            if (isStyle(styleFolder(dataDir, style))) {
                colors = colorSchemeFromText(
                    QString::fromUtf8(readFile(styleFolder(dataDir, style) + QStringLiteral("/colors"))));
                break;
            }
        }
    }
    if (!colorScheme.isEmpty() && validStyleId(colorScheme)) {
        for (const QString &dataDir : dataDirs) {
            const QByteArray scheme =
                readFile(dataDir + QStringLiteral("/color-schemes/") + colorScheme + QStringLiteral(".colors"));
            if (!scheme.isEmpty()) {
                colors = colorSchemeFromText(QString::fromUtf8(scheme));
                break;
            }
        }
    }
    const QString density = configuration.value(QStringLiteral("density")).toString() == QStringLiteral("compact")
                                ? QStringLiteral("compact")
                                : QStringLiteral("comfortable");
    return {
        {QStringLiteral("theme"), theme},
        {QStringLiteral("density"), density},
        {QStringLiteral("tokens"), themeTokensFromText(configuration.value(QStringLiteral("themeTokens")).toString())},
        {QStringLiteral("style"), style},
        {QStringLiteral("meter"), meter},
        {QStringLiteral("background"), background},
        {QStringLiteral("colors"), colors},
        {QStringLiteral("iconTheme"), iconTheme}};
}

} // namespace contribusage
