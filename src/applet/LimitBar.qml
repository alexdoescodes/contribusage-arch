import QtQuick
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kde.ksvg as KSvg
import org.kde.plasma.components as PlasmaComponents

// One usage window: label and percentage, the meter, then the reset text (SPEC §11.2, §11.4).
ColumnLayout {
    property var limit

    spacing: 2
    Accessible.name: limit.label + ", " + limit.percentText + " used, " + limit.resetText

    RowLayout {
        PlasmaComponents.Label { topPadding: Theme.labelInset; bottomPadding: Theme.labelInset; text: limit.label; Layout.fillWidth: true }
        PlasmaComponents.Label { topPadding: Theme.labelInset; bottomPadding: Theme.labelInset; text: limit.percentText; font.features: ({ "tnum": 1 }) }
    }
    // ADR-050: macOS draws SwiftUI's capsule; the Plasma themes draw the theme's meter image, and a warning or critical
    // fill as a capsule in its color over the image's track.
    Item {
        id: meter
        objectName: "meter"
        readonly property bool themed: Theme.meterImage.length > 0 && track.height > 0
        readonly property bool colored: limit.level === "warning" || limit.level === "critical"

        Layout.fillWidth: true
        implicitHeight: themed ? track.height : Theme.meterHeight

        KSvg.Svg {
            id: meterSvg
            imagePath: Theme.meterImage
        }
        KSvg.FrameSvgItem {
            id: track
            visible: meter.themed
            imagePath: Theme.meterImage
            prefix: "bar-inactive"
            width: parent.width
            height: meterSvg.hasElement("hint-bar-size") ? meterSvg.elementSize("hint-bar-size").height
                                                          : fixedMargins.top + fixedMargins.bottom
        }
        KSvg.FrameSvgItem {
            visible: meter.themed && !meter.colored && width >= fixedMargins.left + fixedMargins.right
            imagePath: Theme.meterImage
            prefix: "bar-active"
            width: Math.round(parent.width * limit.fraction)
            height: track.height
        }
        Rectangle {
            visible: !meter.themed
            anchors.fill: parent
            radius: height / 2
            color: Qt.alpha(Kirigami.Theme.textColor, Theme.trackOpacity)
        }
        Rectangle {
            visible: !meter.themed || meter.colored
            width: parent.width * limit.fraction
            height: parent.height
            radius: height / 2
            color: Theme.meterColor(limit.level)
        }
    }
    PlasmaComponents.Label {
        topPadding: Theme.labelInset
        bottomPadding: Theme.labelInset
        visible: limit.resetText.length > 0
        text: limit.resetText
        font: Kirigami.Theme.smallFont
        opacity: Theme.secondaryOpacity
        Layout.alignment: Qt.AlignRight
    }
}
