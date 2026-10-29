import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kcmutils as KCM
import org.kde.kirigami as Kirigami

// Port design §3, ADR-050: theme, density, token overrides, and the folder for QML overrides.
KCM.SimpleKCM {
    property string cfg_theme
    property string cfg_look
    property string cfg_density
    property string cfg_themeTokens

    // System, macOS, then every installed global theme and Plasma style; the choice applies to this widget only.
    readonly property var themes: [{ "id": "system", "name": "System (Plasma)" }, { "id": "macOS", "name": "macOS" }]
        .concat((PopupModel.globalThemes ?? []).map(theme => ({ "id": "global:" + theme.id, "name": "Global theme: " + theme.name })))
        .concat((PopupModel.plasmaStyles ?? []).map(style => ({ "id": "plasma:" + style.id, "name": "Plasma style: " + style.name })))
    // The theme in force: an unset theme follows the old look, as the model does.
    readonly property string shownTheme: cfg_theme.length > 0 ? cfg_theme
        : (cfg_look === "macOS" || cfg_look === "Custom") ? "macOS" : "system"

    Kirigami.FormLayout {
        QQC2.ComboBox {
            Kirigami.FormData.label: "Theme:"
            model: themes
            textRole: "name"
            currentIndex: Math.max(0, themes.findIndex(theme => theme.id === shownTheme))
            onActivated: index => cfg_theme = themes[index].id
            Accessible.name: "Theme"
        }
        QQC2.Label {
            text: "System follows Plasma. macOS draws the macOS app's colors and shapes. A global theme or Plasma style draws its background, meters, colors and icons in this widget only, without changing the desktop."
            wrapMode: Text.Wrap
            opacity: 0.7
            Layout.preferredWidth: Kirigami.Units.gridUnit * 20
        }
        QQC2.ComboBox {
            Kirigami.FormData.label: "Density:"
            model: ["Comfortable", "Compact"]
            currentIndex: cfg_density === "compact" ? 1 : 0
            onActivated: index => cfg_density = index === 1 ? "compact" : "comfortable"
            Accessible.name: "Density"
        }
        QQC2.TextArea {
            Kirigami.FormData.label: "Tokens (JSON):"
            text: cfg_themeTokens
            placeholderText: '{"accent": "#0a84ff", "popupWidth": 400}'
            onEditingFinished: cfg_themeTokens = text
            Layout.preferredWidth: Kirigami.Units.gridUnit * 20
            Layout.preferredHeight: Kirigami.Units.gridUnit * 6
            Accessible.name: "Theme token overrides as JSON"
        }
        QQC2.Label {
            text: "Tokens: accent, warning, critical, claudeHue, githubHue (colors); secondaryOpacity, trackOpacity, emptyCellOpacity, dividerOpacity, mutedBarOpacity (0 to 1); popupWidth, padding, spacing, meterHeight, cellGap, weekChartHeight (pixels). They apply on top of any theme. Unknown or invalid values are ignored."
            wrapMode: Text.Wrap
            opacity: 0.7
            Layout.preferredWidth: Kirigami.Units.gridUnit * 20
        }
        Item { Kirigami.FormData.isSection: true }
        RowLayout {
            Kirigami.FormData.label: "QML overrides:"
            QQC2.Label { text: PopupModel.overrideFolder; elide: Text.ElideMiddle; Layout.maximumWidth: Kirigami.Units.gridUnit * 14 }
            QQC2.Button {
                text: "Open"
                onClicked: Qt.openUrlExternally("file://" + PopupModel.overrideFolder)
            }
        }
        QQC2.Label {
            text: "A file named ProviderGroup.qml, HeatmapBlock.qml or GitHubSection.qml there replaces that block. Overrides run as code inside Plasma; only use files you trust."
            wrapMode: Text.Wrap
            opacity: 0.7
            Layout.preferredWidth: Kirigami.Units.gridUnit * 20
        }
    }
}
