import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.plasma.components as PlasmaComponents
import org.kde.plasma.plasmoid

// Refresh, Settings and Copy as icons with tooltips (SPEC §11.2). A widget has no Quit; Plasma removes it.
RowLayout {
    PlasmaComponents.ToolButton {
        objectName: "refreshButton"
        icon.name: "view-refresh"
        display: QQC2.AbstractButton.IconOnly
        text: "Refresh"
        QQC2.ToolTip.text: text
        QQC2.ToolTip.visible: hovered
        onClicked: PopupModel.refresh()
    }
    Item { Layout.fillWidth: true }
    PlasmaComponents.ToolButton {
        icon.name: "configure"
        display: QQC2.AbstractButton.IconOnly
        text: "Settings"
        QQC2.ToolTip.text: text
        QQC2.ToolTip.visible: hovered
        onClicked: Plasmoid.internalAction("configure").trigger()
    }
    Item { Layout.fillWidth: true }
    PlasmaComponents.ToolButton {
        icon.name: "edit-copy"
        display: QQC2.AbstractButton.IconOnly
        text: "Copy statistics"
        QQC2.ToolTip.text: text
        QQC2.ToolTip.visible: hovered
        // Copy (FR-42) arrives with the real data.
        enabled: false
    }
}
