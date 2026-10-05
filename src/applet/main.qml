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

    // SPEC §12: the popup opening is a trigger for sources whose data is old (ADR-019).
    onExpandedChanged: if (expanded) PopupModel.popupOpened()
}
