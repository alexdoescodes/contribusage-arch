import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kcmutils as KCM
import org.kde.kirigami as Kirigami

// Port design §3: look, density, token overrides, and the folder for QML overrides.
KCM.SimpleKCM {
    property string cfg_look
    property string cfg_density
    property string cfg_themeTokens

    readonly property var looks: ["macOS", "Plasma", "Custom"]

    Kirigami.FormLayout {
        QQC2.ComboBox {
            Kirigami.FormData.label: "Look:"
            model: looks
            currentIndex: Math.max(0, looks.indexOf(cfg_look))
            onActivated: index => cfg_look = looks[index]
            Accessible.name: "Look"
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
            enabled: cfg_look === "Custom"
            text: cfg_themeTokens
            placeholderText: '{"accent": "#0a84ff", "popupWidth": 400}'
            onEditingFinished: cfg_themeTokens = text
            Layout.preferredWidth: Kirigami.Units.gridUnit * 20
            Layout.preferredHeight: Kirigami.Units.gridUnit * 6
            Accessible.name: "Theme token overrides as JSON"
        }
        QQC2.Label {
            text: "Tokens: accent, warning, critical, claudeHue, githubHue (colors); secondaryOpacity, trackOpacity, emptyCellOpacity, dividerOpacity, mutedBarOpacity (0 to 1); popupWidth, padding, spacing, meterHeight, cellGap, weekChartHeight (pixels). Unknown or invalid values are ignored."
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
