import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kde.plasma.components as PlasmaComponents

// The shared heatmap, Combined style (FR-48, FR-49, ADR-032): a legend, then one grid of week columns from Sunday;
// a cell holds a stripe per layer active that day, or the neutral fill.
ColumnLayout {
    property var heatmap
    property string hoveredLine: ""

    spacing: 6

    RowLayout {
        Layout.fillWidth: true
        SectionHeader { title: "Heatmap"; symbol: "view-calendar"; Layout.fillWidth: true }
        Repeater {
            model: heatmap.layers
            RowLayout {
                required property var modelData
                spacing: 4
                Rectangle { width: 8; height: 8; radius: 2; color: Theme.hue(modelData.hue) }
                PlasmaComponents.Label {
                    text: modelData.name
                    font: Kirigami.Theme.smallFont
                    opacity: Theme.secondaryOpacity
                }
            }
        }
    }

    Grid {
        id: grid
        readonly property real side: (width - Theme.cellGap * (heatmap.columns - 1)) / heatmap.columns

        Layout.fillWidth: true
        Layout.preferredHeight: 7 * side + 6 * Theme.cellGap
        rows: 7
        flow: Grid.TopToBottom
        spacing: Theme.cellGap

        Repeater {
            model: heatmap.cells
            Rectangle {
                id: cell
                required property var modelData
                width: grid.side
                height: grid.side
                radius: 2
                clip: true
                color: modelData.stripes.length === 0 ? Qt.alpha(Kirigami.Theme.textColor, Theme.emptyCellOpacity)
                                                      : "transparent"
                Accessible.name: modelData.line

                Row {
                    anchors.fill: parent
                    Repeater {
                        model: modelData.stripes
                        Rectangle {
                            required property var modelData
                            width: grid.side / cell.modelData.stripes.length
                            height: grid.side
                            color: Theme.hue(modelData.hue)
                            opacity: Theme.levelOpacity(modelData.level) * (modelData.dimmed ? 0.5 : 1)
                        }
                    }
                }
                HoverHandler {
                    id: hoverHandler
                    onHoveredChanged: if (hovered) hoveredLine = cell.modelData.line
                }
                QQC2.ToolTip.text: modelData.line
                QQC2.ToolTip.visible: hoverHandler.hovered
                QQC2.ToolTip.delay: 200
            }
        }
    }

    PlasmaComponents.Label {
        text: hoveredLine.length > 0 ? hoveredLine : heatmap.cells[heatmap.cells.length - 1].line
        font: Kirigami.Theme.smallFont
        opacity: Theme.secondaryOpacity
        elide: Text.ElideRight
        Layout.fillWidth: true
    }
}
