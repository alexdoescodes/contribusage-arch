import QtQuick
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kde.plasma.components as PlasmaComponents

// Port design §3, layer 3: one popup block, from the user's override file when there is one, else the built-in
// component. The override gets the block's data as `dataProperty` (and `insightsState` when it declares it); an override
// that fails to load or lacks that property falls back to the built-in block with a notice
// (llm-wiki/guides/qml-overrides.md).
ColumnLayout {
    id: slot

    property string blockName
    property url overrideSource
    property bool hasData: true
    property Component builtIn
    property string dataProperty // "provider", "heatmap" or "github"
    property var blockData
    property QtObject insightsState: null
    property bool overrideFailed: false
    readonly property bool usesOverride: overrideSource.toString().length > 0 && !overrideFailed

    spacing: 0

    PlasmaComponents.Label {
        objectName: "overrideNotice"
        visible: slot.overrideFailed
        text: "Custom " + slot.blockName + " failed to load; showing the built-in one."
        font: Kirigami.Theme.smallFont
        opacity: Theme.secondaryOpacity
        wrapMode: Text.Wrap
        Layout.fillWidth: true
    }
    Loader {
        // Declarative, so data that arrives after the slot exists still loads the override.
        active: slot.hasData && slot.usesOverride
        source: slot.usesOverride ? slot.overrideSource : ""
        visible: status === Loader.Ready
        Layout.fillWidth: true
        onStatusChanged: if (status === Loader.Error) slot.overrideFailed = true
        onLoaded: {
            if (!(slot.dataProperty in item)) {
                // Not this loader's own handler: unloading the item it is handing over waits a turn.
                Qt.callLater(() => slot.overrideFailed = true);
                return;
            }
            item[slot.dataProperty] = Qt.binding(() => slot.blockData);
            if ("insightsState" in item) {
                item.insightsState = Qt.binding(() => slot.insightsState);
            }
        }
    }
    Loader {
        active: slot.hasData && !slot.usesOverride
        sourceComponent: slot.builtIn
        Layout.fillWidth: true
    }
}
