import QtQuick
import QtQuick.Shapes
import org.kde.kirigami as Kirigami

// ADR-035's mark: a prompt, then three code lines the agent "types" as the window fills. Drawn in an 18 × 16 design box
// (MenuBarArt.swift's units) and scaled to the item's height.
Item {
    id: glyph

    property real fraction: 0
    property color fillColor: Theme.accent
    readonly property real unit: height / 18
    // Code lines as (x, length) in design units, top to bottom; their y is 3.5, 9 and 14.5.
    readonly property var lines: [[6.7, 9.0], [3.7, 12.0], [3.7, 7.5]]

    implicitHeight: Kirigami.Units.iconSizes.small
    implicitWidth: 16 * unit

    Shape {
        anchors.fill: parent
        preferredRendererType: Shape.CurveRenderer

        ShapePath {
            strokeColor: Kirigami.Theme.textColor
            strokeWidth: 1.8 * glyph.unit
            fillColor: "transparent"
            capStyle: ShapePath.RoundCap
            joinStyle: ShapePath.RoundJoin
            startX: 1 * glyph.unit
            startY: (3.5 - 2.8) * glyph.unit
            PathLine { x: (1 + 2.94) * glyph.unit; y: 3.5 * glyph.unit }
            PathLine { x: 1 * glyph.unit; y: (3.5 + 2.8) * glyph.unit }
        }
    }

    Repeater {
        model: 3
        Item {
            required property int index
            readonly property real lineX: glyph.lines[index][0]
            readonly property real length: glyph.lines[index][1]
            // Fill bar after bar, up to `fraction` of their total length.
            readonly property real before: index === 0 ? 0 : index === 1 ? 9.0 : 21.0
            readonly property real filled: Math.max(0, Math.min(length, glyph.fraction * 28.5 - before))

            x: lineX * glyph.unit
            y: (3.5 + 5.5 * index - 1.1) * glyph.unit
            width: length * glyph.unit
            height: 2.2 * glyph.unit

            Rectangle {
                anchors.fill: parent
                radius: height / 2
                color: Qt.alpha(Kirigami.Theme.textColor, 0.25)
            }
            Rectangle {
                visible: parent.filled > 0
                width: parent.filled * glyph.unit
                height: parent.height
                radius: height / 2
                color: glyph.fillColor
            }
        }
    }
}
