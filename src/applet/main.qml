import QtQuick
import org.kde.kirigami as Kirigami
import org.kde.plasma.plasmoid

PlasmoidItem {
    id: root

    switchWidth: Theme.popupWidth
    switchHeight: Kirigami.Units.gridUnit * 20

    compactRepresentation: CompactLabel {
        onToggled: root.expanded = !root.expanded
    }

    fullRepresentation: FullPopup {}
}
