import QtQuick
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

// SPEC §11.2: the blocks in the default order, separated by dividers, then the footer.
ColumnLayout {
    id: popup

    // The Insights open state and period, kept while the provider rows are rebuilt (one provider so far).
    property QtObject insightsState: QtObject {
        property bool expanded: false
        property int period: 0
    }

    implicitWidth: Theme.popupWidth
    Layout.preferredWidth: Theme.popupWidth
    spacing: 0

    Repeater {
        model: PopupModel.providers
        ProviderGroup {
            required property var modelData
            provider: modelData
            insightsState: popup.insightsState
            Layout.fillWidth: true
            Layout.margins: Theme.padding
        }
    }
    Rectangle {
        visible: heatmapLoader.active
        Layout.fillWidth: true
        implicitHeight: 1
        color: Qt.alpha(Kirigami.Theme.textColor, Theme.dividerOpacity)
    }
    Loader {
        id: heatmapLoader
        active: PopupModel.heatmap.cells !== undefined
        visible: active
        Layout.fillWidth: true
        Layout.margins: Theme.padding
        sourceComponent: HeatmapBlock { heatmap: PopupModel.heatmap }
    }
    Rectangle {
        visible: githubLoader.active
        Layout.fillWidth: true
        implicitHeight: 1
        color: Qt.alpha(Kirigami.Theme.textColor, Theme.dividerOpacity)
    }
    Loader {
        id: githubLoader
        active: PopupModel.github.login !== undefined
        visible: active
        Layout.fillWidth: true
        Layout.margins: Theme.padding
        sourceComponent: GitHubSection { github: PopupModel.github }
    }
    Rectangle { Layout.fillWidth: true; implicitHeight: 1; color: Qt.alpha(Kirigami.Theme.textColor, Theme.dividerOpacity) }
    Footer {
        Layout.fillWidth: true
        Layout.margins: Theme.spacing
    }
}
