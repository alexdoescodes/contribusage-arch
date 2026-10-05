import QtQuick
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kde.plasma.components as PlasmaComponents
import org.kde.plasma.plasmoid

// SPEC §11.2, FR-44: the visible blocks in the layout's order, separated by dividers, then the footer; "Nothing to
// show" while no visible block has a source that is on (FR-47).
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
        model: PopupModel.blocks
        ColumnLayout {
            id: block

            required property string modelData
            required property int index
            // A block whose data has not arrived yet draws nothing, as before the layout existed.
            readonly property bool hasData: modelData === "heatmap" ? PopupModel.heatmap.cells !== undefined
                : modelData === "github" ? (PopupModel.github.connected !== undefined || PopupModel.github.login !== undefined)
                : true

            visible: hasData
            spacing: 0
            Layout.fillWidth: true

            Rectangle {
                visible: block.index > 0
                Layout.fillWidth: true
                implicitHeight: 1
                color: Qt.alpha(Kirigami.Theme.textColor, Theme.dividerOpacity)
            }
            Loader {
                active: block.hasData
                Layout.fillWidth: true
                Layout.margins: Theme.padding
                sourceComponent: block.modelData === "heatmap" ? heatmapBlock
                    : block.modelData === "github" ? githubBlock : providerBlock
            }
        }
    }
    ColumnLayout {
        visible: PopupModel.nothingToShow
        Layout.fillWidth: true
        Layout.margins: Theme.padding
        PlasmaComponents.Label {
            text: "Nothing to show"
            opacity: Theme.secondaryOpacity
            Layout.alignment: Qt.AlignHCenter
        }
        PlasmaComponents.Button {
            text: "Open Settings…"
            Layout.alignment: Qt.AlignHCenter
            onClicked: Plasmoid.internalAction("configure").trigger()
        }
    }
    Rectangle { Layout.fillWidth: true; implicitHeight: 1; color: Qt.alpha(Kirigami.Theme.textColor, Theme.dividerOpacity) }
    Footer {
        Layout.fillWidth: true
        Layout.margins: Theme.spacing
    }

    Component {
        id: providerBlock
        ProviderGroup {
            provider: PopupModel.providers[0]
            insightsState: popup.insightsState
        }
    }
    Component {
        id: heatmapBlock
        HeatmapBlock { heatmap: PopupModel.heatmap }
    }
    Component {
        id: githubBlock
        GitHubSection {
            github: PopupModel.github
            onTokenSubmitted: token => PopupModel.connectGitHub(token)
        }
    }
}
