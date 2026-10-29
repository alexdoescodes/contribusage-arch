pragma Singleton
import QtQuick
import org.kde.kirigami as Kirigami

// Every color, shape and size the widget draws with (port design §3, layer 2; ADR-050). The theme picks the base:
// "system" (Plasma's colors and meter image, square cells), "macOS" (the macOS app's colors and shapes: pill meters,
// 2 px corners) or "plasma:<id>" (an installed Plasma style, for this widget only: its dialog background, meter image
// and color scheme, falling back to Plasma's colors where the style has none). The user's tokens apply on top of any
// theme. Density scales the spacing. Text colors come from Kirigami's theme, which FullPopup sets from `styleColors`.
QtObject {
    id: root

    readonly property var appearance: PopupModel.appearance ?? ({})
    readonly property string theme: appearance.theme ?? "system"
    readonly property var tokens: appearance.tokens ?? ({})
    readonly property real densityScale: appearance.density === "compact" ? 0.7 : 1

    function token(name, fallback) {
        return tokens[name] !== undefined ? tokens[name] : fallback;
    }

    readonly property bool macOS: theme === "macOS"
    // macOS follows the system's light or dark scheme, as the app's popover does.
    readonly property bool dark: Kirigami.ColorUtils.brightnessForColor(Kirigami.Theme.backgroundColor) === Kirigami.ColorUtils.Dark
    // The macOS popover: its own rounded card over the popup window, in the app's label colors; see-through like the
    // app's glass, the desktop's blur behind it.
    readonly property color popoverBackground: dark ? Qt.rgba(0.12, 0.12, 0.14, 0.62) : Qt.rgba(0.97, 0.97, 0.98, 0.8)
    readonly property color popoverBorder: dark ? Qt.rgba(1, 1, 1, 0.14) : Qt.rgba(0, 0, 0, 0.1)
    readonly property int popoverRadius: 12
    // The macOS app's label colors in dark and light mode, as #rrggbb strings.
    readonly property var macDarkColors: ({ "textColor": "#e6e6e8", "backgroundColor": "#212124", "highlightColor": "#0a84ff",
                                            "negativeTextColor": "#ff453a", "neutralTextColor": "#ff9f0a", "positiveTextColor": "#30d158" })
    readonly property var macLightColors: ({ "textColor": "#1d1d1f", "backgroundColor": "#f5f5f7", "highlightColor": "#007aff",
                                             "negativeTextColor": "#ff3b30", "neutralTextColor": "#ff9500", "positiveTextColor": "#34c759" })
    readonly property var macColors: dark ? macDarkColors : macLightColors
    // The colors FullPopup gives every item: a style's color scheme or the macOS ones, as #rrggbb strings
    // (textColor, backgroundColor, highlightColor, negative/neutral/positiveTextColor); empty for the system theme.
    readonly property var styleColors: macOS ? macColors : (appearance.colors ?? ({}))
    // The style's dialog background file, drawn behind the popup's blocks; empty unless a style is chosen.
    readonly property string backgroundImage: appearance.background ?? ""
    // Plasma's meter image (theme-relative for the system theme, a file for a style); empty draws the macOS capsule.
    readonly property string meterImage: appearance.meter ?? ""
    // SwiftUI's `cornerRadius(2)` on cells, stripes, 7-day bars and legend swatches; Plasma's views draw them square.
    readonly property int cornerRadius: macOS ? 2 : 0
    // Noto Sans lines are 2 px taller than the macOS app's SF Pro ones; macOS labels drop a pixel above and below
    // (Text's lineHeight does not shrink a single line).
    readonly property int labelInset: macOS ? -1 : 0

    // ADR-050: a Breeze icon name, or under macOS the widget's drawing of the macOS app's symbol for it.
    readonly property var macSymbols: ({
        "draw-star": "sparkle", "view-calendar": "calendar-dots", "vcs-commit": "grid", "view-refresh": "arrow-clockwise",
        "configure": "gearshape", "edit-copy": "doc-on-doc", "arrow-right": "chevron-right", "arrow-down": "chevron-down"
    })
    // The theme's icon theme: a global theme's own; macOS's WhiteSur in the system's brightness.
    readonly property string iconTheme: macOS ? (dark ? "WhiteSur-dark" : "WhiteSur-light") : (appearance.iconTheme ?? "")
    // Under macOS the widget's drawing of the app's symbol, in the label color; else the file of the icon in the
    // theme's icon theme (macOS: for icons without a drawing); empty keeps the desktop's icon of that name.
    function iconSource(name) {
        if (macOS && macSymbols[name] !== undefined) {
            return PopupModel.symbolFile(macSymbols[name], macColors.textColor);
        }
        const file = iconTheme.length > 0 ? PopupModel.iconFile(iconTheme, name) : "";
        return file.length > 0 ? "file://" + file : "";
    }
    function icon(name) {
        return iconSource(name) || name;
    }

    function platform(styleName, kirigamiColor) {
        return styleColors[styleName] !== undefined ? styleColors[styleName] : kirigamiColor;
    }

    readonly property color accent: token("accent", macOS ? "#0a84ff" : platform("highlightColor", Kirigami.Theme.highlightColor))
    // SPEC §11.4: system yellow/orange, red and green; in the Plasma themes the theme's neutral, negative and positive.
    readonly property color warning: token("warning", macOS ? "#ff9f0a" : platform("neutralTextColor", Kirigami.Theme.neutralTextColor))
    readonly property color critical: token("critical", macOS ? "#ff453a" : platform("negativeTextColor", Kirigami.Theme.negativeTextColor))
    readonly property color claudeHue: token("claudeHue", macOS ? "#ff9f0a" : platform("neutralTextColor", Kirigami.Theme.neutralTextColor))
    readonly property color githubHue: token("githubHue", macOS ? "#30d158" : platform("positiveTextColor", Kirigami.Theme.positiveTextColor))

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
