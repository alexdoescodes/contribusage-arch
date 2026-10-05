import QtQuick
import QtQuick.Shapes
import org.kde.kirigami as Kirigami

// FR-51 `ring` and `rings`: the shown window as an arc; `inner` ≥ 0 adds the provider's other window inside.
Item {
    id: ring

    property real fraction: 0
    property real inner: -1
    property color fillColor: Theme.accent
    property string centerText: ""

    implicitHeight: Kirigami.Units.iconSizes.small
    implicitWidth: implicitHeight

    Shape {
        anchors.fill: parent
        preferredRendererType: Shape.CurveRenderer
        ShapePath {
            strokeColor: Qt.alpha(Kirigami.Theme.textColor, Theme.trackOpacity)
            strokeWidth: ring.height * 0.14
            fillColor: "transparent"
            PathAngleArc { centerX: ring.width / 2; centerY: ring.height / 2; radiusX: ring.height * 0.42; radiusY: radiusX; startAngle: 0; sweepAngle: 360 }
        }
        ShapePath {
            strokeColor: ring.fillColor
            strokeWidth: ring.height * 0.14
            fillColor: "transparent"
            capStyle: ShapePath.RoundCap
            PathAngleArc { centerX: ring.width / 2; centerY: ring.height / 2; radiusX: ring.height * 0.42; radiusY: radiusX; startAngle: -90; sweepAngle: 360 * Math.max(0, Math.min(1, ring.fraction)) }
        }
        ShapePath {
            strokeColor: ring.inner >= 0 ? Qt.alpha(ring.fillColor, 0.6) : "transparent"
            strokeWidth: ring.height * 0.12
            fillColor: "transparent"
            capStyle: ShapePath.RoundCap
            PathAngleArc { centerX: ring.width / 2; centerY: ring.height / 2; radiusX: ring.height * 0.22; radiusY: radiusX; startAngle: -90; sweepAngle: 360 * Math.max(0, Math.min(1, ring.inner)) }
        }
    }
    Text {
        anchors.centerIn: parent
        visible: ring.centerText.length > 0
        text: ring.centerText
        color: Kirigami.Theme.textColor
        font.pixelSize: ring.height * 0.36
        font.features: ({ "tnum": 1 })
    }
}
