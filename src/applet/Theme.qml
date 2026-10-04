pragma Singleton
import QtQuick

// Every color and size the widget draws with (port design §3, layer 2). The defaults are the macOS app's look; the
// customization layers (L-6) override these properties. Text colors come from Kirigami's theme in each component.
QtObject {
    readonly property color accent: "#0a84ff"
    readonly property color warning: "#ff9f0a"
    readonly property color critical: "#ff453a"
    readonly property color claudeHue: "#ff9f0a"
    readonly property color githubHue: "#30d158"

    // Opacities over the theme's text color.
    readonly property real secondaryOpacity: 0.6
    readonly property real trackOpacity: 0.15
    readonly property real emptyCellOpacity: 0.12
    readonly property real dividerOpacity: 0.12
    readonly property real mutedBarOpacity: 0.45

    readonly property int popupWidth: 360
    readonly property int padding: 12
    readonly property int spacing: 8
    readonly property int meterHeight: 6
    readonly property int cellGap: 3
    readonly property int weekChartHeight: 32

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
}
