import QtQuick
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

// SPEC §11.2: the blocks in the default order, separated by dividers, then the footer.
ColumnLayout {
    implicitWidth: Theme.popupWidth
    Layout.preferredWidth: Theme.popupWidth
    spacing: 0

    Repeater {
        model: PopupModel.providers
        ProviderGroup {
            required property var modelData
            provider: modelData
            Layout.fillWidth: true
            Layout.margins: Theme.padding
        }
    }
    Rectangle { Layout.fillWidth: true; implicitHeight: 1; color: Qt.alpha(Kirigami.Theme.textColor, Theme.dividerOpacity) }
    HeatmapBlock {
        heatmap: PopupModel.heatmap
        Layout.fillWidth: true
        Layout.margins: Theme.padding
    }
    Rectangle { Layout.fillWidth: true; implicitHeight: 1; color: Qt.alpha(Kirigami.Theme.textColor, Theme.dividerOpacity) }
    GitHubSection {
        github: PopupModel.github
        Layout.fillWidth: true
        Layout.margins: Theme.padding
    }
    Rectangle { Layout.fillWidth: true; implicitHeight: 1; color: Qt.alpha(Kirigami.Theme.textColor, Theme.dividerOpacity) }
    Footer {
        Layout.fillWidth: true
        Layout.margins: Theme.spacing
    }
}
