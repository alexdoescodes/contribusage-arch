import QtQuick
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kde.plasma.components as PlasmaComponents
import org.kde.plasma.core as PlasmaCore
import org.kde.plasma.plasmoid

// SPEC §11.1 in the `prompt` style: the mark filled with the session window, then its value; a vertical panel has
// room for the mark only.
MouseArea {
    id: label

    readonly property bool vertical: Plasmoid.formFactor === PlasmaCore.Types.Vertical

    signal toggled

    Layout.minimumWidth: content.implicitWidth
    Layout.preferredWidth: content.implicitWidth
    Layout.minimumHeight: vertical ? Kirigami.Units.iconSizes.small : 0
    Accessible.name: content.Accessible.name
    onClicked: toggled()

    LabelContent {
        id: content
        anchors.centerIn: parent
        style: Plasmoid.configuration.menuBarStyle ?? "prompt"
        tint: Plasmoid.configuration.menuBarTint ?? "provider"
        customColor: Plasmoid.configuration.menuBarColor ?? Theme.claudeHue
        label: PopupModel.label
        heatmap: PopupModel.heatmap
        vertical: label.vertical
    }

}
