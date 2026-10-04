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
    readonly property real glyphHeight: Math.min(height, Kirigami.Units.iconSizes.smallMedium)

    signal toggled

    Layout.minimumWidth: row.implicitWidth
    Layout.preferredWidth: row.implicitWidth
    Layout.minimumHeight: vertical ? glyphHeight : 0
    Accessible.name: "Claude Code " + PopupModel.label.text
    onClicked: toggled()

    RowLayout {
        id: row
        anchors.centerIn: parent
        spacing: 4

        PromptGlyph {
            fraction: PopupModel.label.fraction
            fillColor: Theme.meterColor(PopupModel.label.level)
            Layout.preferredHeight: label.glyphHeight
            Layout.preferredWidth: implicitWidth
        }
        PlasmaComponents.Label {
            visible: !label.vertical
            text: PopupModel.label.text
            font.features: ({ "tnum": 1 })
        }
    }
}
