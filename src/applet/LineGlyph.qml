import QtQuick
import org.kde.kirigami as Kirigami

// FR-51 `line`: a prompt and a cursor line that fills.
Row {
    id: line

    property real fraction: 0
    property color fillColor: Theme.accent

    // A fixed size, not the item's height: in a layout the height follows the text, and a text sized by the height
    // would grow both without end.
    readonly property real size: Kirigami.Units.iconSizes.small

    spacing: 2

    Text {
        text: "›"
        color: Kirigami.Theme.textColor
        font.pixelSize: line.size
        anchors.verticalCenter: parent.verticalCenter
    }
    Rectangle {
        width: line.size * 1.4
        height: line.size * 0.22
        radius: height / 2
        anchors.verticalCenter: parent.verticalCenter
        color: Qt.alpha(Kirigami.Theme.textColor, Theme.trackOpacity)
        Rectangle {
            width: parent.width * Math.max(0, Math.min(1, line.fraction))
            height: parent.height
            radius: parent.radius
            color: line.fillColor
        }
    }
}
