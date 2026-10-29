import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kde.plasma.components as PlasmaComponents
import org.kde.plasma.plasmoid

// Refresh, Settings and Copy as icons with tooltips (SPEC §11.2). A widget has no Quit; Plasma removes it.
RowLayout {
    PlasmaComponents.ToolButton {
        objectName: "refreshButton"
        icon.name: Theme.iconSource("view-refresh").length > 0 ? "" : "view-refresh"
        icon.source: Theme.iconSource("view-refresh")
        icon.color: Kirigami.Theme.textColor
        // SwiftUI's `.imageScale(.large)` symbols under macOS.
        icon.width: Theme.macOS ? 22 : Kirigami.Units.iconSizes.smallMedium
        icon.height: Theme.macOS ? 22 : Kirigami.Units.iconSizes.smallMedium
        display: QQC2.AbstractButton.IconOnly
        text: "Refresh"
        QQC2.ToolTip.text: text
        QQC2.ToolTip.visible: hovered
        onClicked: PopupModel.refresh()
    }
    Item { Layout.fillWidth: true }
    PlasmaComponents.ToolButton {
        icon.name: Theme.iconSource("configure").length > 0 ? "" : "configure"
        icon.source: Theme.iconSource("configure")
        icon.color: Kirigami.Theme.textColor
        // SwiftUI's `.imageScale(.large)` symbols under macOS.
        icon.width: Theme.macOS ? 22 : Kirigami.Units.iconSizes.smallMedium
        icon.height: Theme.macOS ? 22 : Kirigami.Units.iconSizes.smallMedium
        display: QQC2.AbstractButton.IconOnly
        text: "Settings"
        QQC2.ToolTip.text: text
        QQC2.ToolTip.visible: hovered
        onClicked: Plasmoid.internalAction("configure").trigger()
    }
    Item { Layout.fillWidth: true }
    PlasmaComponents.ToolButton {
        icon.name: Theme.iconSource("edit-copy").length > 0 ? "" : "edit-copy"
        icon.source: Theme.iconSource("edit-copy")
        icon.color: Kirigami.Theme.textColor
        // SwiftUI's `.imageScale(.large)` symbols under macOS.
        icon.width: Theme.macOS ? 22 : Kirigami.Units.iconSizes.smallMedium
        icon.height: Theme.macOS ? 22 : Kirigami.Units.iconSizes.smallMedium
        display: QQC2.AbstractButton.IconOnly
        text: "Copy statistics"
        QQC2.ToolTip.text: text
        QQC2.ToolTip.visible: hovered
        // Copy (FR-42) arrives with the real data.
        enabled: false
    }
}
