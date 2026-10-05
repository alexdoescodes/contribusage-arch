#include "core/appearance.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>

#include <algorithm>

namespace contribusage {

namespace {

struct Range {
    double low;
    double high;
};

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

QVariantMap appearanceFromMap(const QVariantMap &configuration) {
    QString look = configuration.value(QStringLiteral("look")).toString();
    if (look != QStringLiteral("Plasma") && look != QStringLiteral("Custom")) {
        look = QStringLiteral("macOS");
    }
    const QString density = configuration.value(QStringLiteral("density")).toString() == QStringLiteral("compact")
                                ? QStringLiteral("compact")
                                : QStringLiteral("comfortable");
    const QVariantMap tokens = look == QStringLiteral("Custom")
                                   ? themeTokensFromText(configuration.value(QStringLiteral("themeTokens")).toString())
                                   : QVariantMap();
    return {{QStringLiteral("look"), look}, {QStringLiteral("density"), density}, {QStringLiteral("tokens"), tokens}};
}

} // namespace contribusage
