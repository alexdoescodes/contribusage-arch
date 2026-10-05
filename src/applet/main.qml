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

    // SPEC §10.7: the settings live in the plasmoid configuration; the model applies them at start and on change.
    function applySettings() {
        const c = Plasmoid.configuration;
        PopupModel.applySettings({
            "menuBarMode": c.menuBarMode,
            "notificationThresholds": c.notificationThresholds,
            "notifyOnReset": c.notifyOnReset,
            "probeIntervalMinutes": c.probeIntervalMinutes,
            "claudePathOverride": c.claudePathOverride,
            "githubEnabled": c.githubEnabled,
            "githubIntervalMinutes": c.githubIntervalMinutes,
            "popoverLayout": c.popoverLayout,
            "menuBarStyle": c.menuBarStyle
        });
    }
    Component.onCompleted: applySettings()
    Connections {
        target: Plasmoid.configuration
        function onValueChanged() { root.applySettings(); }
    }

    // SPEC §12: the popup opening is a trigger for sources whose data is old (ADR-019).
    onExpandedChanged: if (expanded) PopupModel.popupOpened()
}
