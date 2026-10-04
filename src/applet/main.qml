import QtQuick
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kde.plasma.components as PlasmaComponents
import org.kde.plasma.plasmoid

// Placeholder representations until M4 brings the drawn label and the popup blocks.
PlasmoidItem {
    id: root

    compactRepresentation: PlasmaComponents.Label {
        text: "contribusage"

        MouseArea {
            anchors.fill: parent
            onClicked: root.expanded = !root.expanded
        }
    }

    fullRepresentation: ColumnLayout {
        Layout.preferredWidth: Kirigami.Units.gridUnit * 20

        PlasmaComponents.Label {
            text: "contribusage " + AppInfo.version
        }
    }
}
