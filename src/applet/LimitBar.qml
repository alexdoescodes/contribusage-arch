import QtQuick
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kde.plasma.components as PlasmaComponents

// One usage window: label and percentage, the meter, then the reset text (SPEC §11.2, §11.4).
ColumnLayout {
    property var limit

    spacing: 2
    Accessible.name: limit.label + ", " + limit.percentText + " used, " + limit.resetText

    RowLayout {
        PlasmaComponents.Label { text: limit.label; Layout.fillWidth: true }
        PlasmaComponents.Label { text: limit.percentText; font.features: ({ "tnum": 1 }) }
    }
    Rectangle {
        Layout.fillWidth: true
        implicitHeight: Theme.meterHeight
        radius: height / 2
        color: Qt.alpha(Kirigami.Theme.textColor, Theme.trackOpacity)

        Rectangle {
            width: parent.width * limit.fraction
            height: parent.height
            radius: parent.radius
            color: Theme.meterColor(limit.level)
        }
    }
    PlasmaComponents.Label {
        visible: limit.resetText.length > 0
        text: limit.resetText
        font: Kirigami.Theme.smallFont
        opacity: Theme.secondaryOpacity
        Layout.alignment: Qt.AlignRight
    }
}
