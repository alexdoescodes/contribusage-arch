pragma Singleton
import QtQuick
import org.kde.kirigami as Kirigami

// Every color and size the widget draws with (port design §3, layer 2). The look picks the base: macOS (the defaults
// below), Plasma (the platform theme: Kirigami's highlight and status colors) or Custom (the defaults, with the user's token overrides). Density scales the
// spacing. Text colors come from Kirigami's theme in each component.
QtObject {
    id: theme

    readonly property var appearance: PopupModel.appearance ?? ({})
    readonly property string look: appearance.look ?? "macOS"
    readonly property var tokens: appearance.tokens ?? ({})
    readonly property real densityScale: appearance.density === "compact" ? 0.7 : 1

    function token(name, fallback) {
        return tokens[name] !== undefined ? tokens[name] : fallback;
    }

    readonly property bool plasma: look === "Plasma"
    readonly property color accent: plasma ? Kirigami.Theme.highlightColor : token("accent", "#0a84ff")
    // SPEC §11.4: system yellow/orange, red and green; in the Plasma look the theme's neutral, negative and positive.
    readonly property color warning: plasma ? Kirigami.Theme.neutralTextColor : token("warning", "#ff9f0a")
    readonly property color critical: plasma ? Kirigami.Theme.negativeTextColor : token("critical", "#ff453a")
    readonly property color claudeHue: plasma ? Kirigami.Theme.neutralTextColor : token("claudeHue", "#ff9f0a")
    readonly property color githubHue: plasma ? Kirigami.Theme.positiveTextColor : token("githubHue", "#30d158")

    // Opacities over the theme's text color.
    readonly property real secondaryOpacity: token("secondaryOpacity", 0.6)
    readonly property real trackOpacity: token("trackOpacity", 0.15)
    readonly property real emptyCellOpacity: token("emptyCellOpacity", 0.12)
    readonly property real dividerOpacity: token("dividerOpacity", 0.12)
    readonly property real mutedBarOpacity: token("mutedBarOpacity", 0.45)

    readonly property int popupWidth: token("popupWidth", 360)
    readonly property int padding: Math.round(token("padding", 12) * densityScale)
    readonly property int spacing: Math.round(token("spacing", 8) * densityScale)
    readonly property int meterHeight: token("meterHeight", 6)
    readonly property int cellGap: token("cellGap", 3)
    readonly property int weekChartHeight: Math.round(token("weekChartHeight", 32) * densityScale)

    function meterColor(level) {
        return level === "critical" ? critical : level === "warning" ? warning : accent;
    }

    function hue(name) {
        return name === "github" ? githubHue : claudeHue;
    }

    // SPEC §11.4: levels 1 to 4 from 40 % to full strength.
    function levelOpacity(level) {
        return 0.2 + 0.2 * level;
    }

    // FR-51: the label's color; from 90 % red in every tint but monochrome.
    function labelColor(tint, level, hueName, customColor, textColor, accentColor) {
        if (tint === "monochrome") {
            return textColor;
        }
        if (level === "critical") {
            return critical;
        }
        if (tint === "usage") {
            return meterColor(level);
        }
        if (tint === "accent") {
            return accentColor;
        }
        if (tint === "custom") {
            return customColor;
        }
        return hue(hueName);
    }
}
