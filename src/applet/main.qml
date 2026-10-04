import QtQuick
import org.kde.kirigami as Kirigami
import org.kde.plasma.components as PlasmaComponents
import org.kde.plasma.plasmoid

PlasmoidItem {
    id: root

    switchWidth: Theme.popupWidth
    switchHeight: Kirigami.Units.gridUnit * 20

    compactRepresentation: PlasmaComponents.Label {
        text: "contribusage"

        MouseArea {
            anchors.fill: parent
            onClicked: root.expanded = !root.expanded
        }
    }

    fullRepresentation: FullPopup {}
}
