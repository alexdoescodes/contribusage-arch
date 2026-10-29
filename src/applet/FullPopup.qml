import QtQuick
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kde.ksvg as KSvg
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

    // ADR-050: a Plasma style chosen for this widget lends its colors to everything in the popup.
    StyleTint {
        // In a Plasma popup window the window's own frame too.
        target: popup.Window.window !== null && popup.Window.window.topPadding !== undefined ? popup.Window.contentItem : popup
        colors: Theme.styleColors
        style: Theme.appearance.style ?? ""
        hideFrame: Theme.macOS
    }

    // ADR-050: and its dialog background, or the macOS popover's rounded card, drawn over the whole popup window (the
    // frame the shell draws with its own style included); outside a Plasma popup window, over the popup only. A
    // zero-size layout item, so it takes no room.
    Item {
        id: backdrop

        // A PlasmaWindow has paddings; the viewer's or a desktop's window does not.
        readonly property bool ownWindow: popup.Window.window !== null && popup.Window.window.topPadding !== undefined
        readonly property point origin: ownWindow && popup.width > 0 && popup.height > 0
                                        && popup.Window.width > 0 ? popup.mapToItem(null, 0, 0) : Qt.point(0, 0)
        readonly property rect area: Qt.rect(-origin.x, -origin.y, ownWindow ? popup.Window.width : popup.width,
                                             ownWindow ? popup.Window.height : popup.height)

        Layout.preferredWidth: 0
        Layout.preferredHeight: 0
        z: -1

        KSvg.FrameSvgItem {
            objectName: "styleBackground"
            visible: Theme.backgroundImage.length > 0
            imagePath: Theme.backgroundImage
            x: backdrop.area.x
            y: backdrop.area.y
            width: backdrop.area.width
            height: backdrop.area.height
        }
        Rectangle {
            objectName: "macOSPopover"
            visible: Theme.macOS
            x: backdrop.area.x
            y: backdrop.area.y
            width: backdrop.area.width
            height: backdrop.area.height
            radius: Theme.popoverRadius
            color: Theme.popoverBackground
            border.width: 1
            border.color: Theme.popoverBorder
        }
    }

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
            OverridableBlock {
                blockName: block.modelData === "heatmap" ? "HeatmapBlock"
                    : block.modelData === "github" ? "GitHubSection" : "ProviderGroup"
                overrideSource: PopupModel.overrideUrl(blockName)
                hasData: block.hasData
                builtIn: block.modelData === "heatmap" ? heatmapBlock
                    : block.modelData === "github" ? githubBlock : providerBlock
                dataProperty: block.modelData === "heatmap" ? "heatmap" : block.modelData === "github" ? "github" : "provider"
                blockData: block.modelData === "heatmap" ? PopupModel.heatmap
                    : block.modelData === "github" ? PopupModel.github : PopupModel.providers[0]
                insightsState: popup.insightsState
                Layout.fillWidth: true
                Layout.margins: Theme.padding
            }
        }
    }
    ColumnLayout {
        visible: PopupModel.nothingToShow
        Layout.fillWidth: true
        Layout.margins: Theme.padding
        PlasmaComponents.Label {
            topPadding: Theme.labelInset
            bottomPadding: Theme.labelInset
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
