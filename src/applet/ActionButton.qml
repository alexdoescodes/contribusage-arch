import QtQuick
import org.kde.plasma.components as PlasmaComponents
import org.kde.plasma.plasmoid

// SPEC §11.3's one action per state: Retry (refresh now), Locate… (Settings), Show raw output.
PlasmaComponents.Button {
    objectName: "actionButton"
    property var stateAction

    signal raw

    visible: stateAction !== undefined
    text: stateAction ? stateAction.text : ""
    Accessible.name: text
    onClicked: {
        if (stateAction.kind === "retry") {
            PopupModel.refresh();
        } else if (stateAction.kind === "configure") {
            Plasmoid.internalAction("configure").trigger();
        } else if (stateAction.kind === "raw") {
            raw();
        }
    }
}
