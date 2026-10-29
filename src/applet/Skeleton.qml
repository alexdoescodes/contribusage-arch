import QtQuick
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

// SPEC §11.3 first load: placeholder bars in the shape of the content to come.
ColumnLayout {
    objectName: "skeleton"
    property int rows: 2

    spacing: Theme.spacing
    Accessible.name: "Loading"

    Repeater {
        model: rows
        Rectangle {
            required property int index
            Layout.fillWidth: true
            Layout.rightMargin: index % 2 === 1 ? parent.width * 0.3 : 0
            implicitHeight: Theme.meterHeight * 2
            radius: Theme.meterHeight
            color: Qt.alpha(Kirigami.Theme.textColor, Theme.trackOpacity)
        }
    }
}
