import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kde.plasma.components as PlasmaComponents

// The shared heatmap (FR-48, FR-49, ADR-032): a legend, then one grid of week columns from Sunday; a cell holds a
// stripe per layer active that day, or the neutral fill. Stacked draws one grid per layer, the first one walkable.
ColumnLayout {
    id: block

    property var heatmap
    property int focusedIndex: -1
    readonly property int lastIndex: (heatmap.cells ?? []).length - 1

    // The cells change with the day and the data; a focused day past the end moves to the last one.
    onLastIndexChanged: if (focusedIndex > lastIndex) focusedIndex = lastIndex

    function lineAt(index) {
        const cells = heatmap.cells ?? [];
        return index >= 0 && index < cells.length ? cells[index].line : "";
    }
    property string hoveredLine: ""
    readonly property bool stacked: heatmap.style === "stacked" && (heatmap.layers ?? []).length > 1
    readonly property string firstHue: stacked ? heatmap.layers[0].hue : ""

    function stripesOf(cell, hue) {
        return hue.length === 0 ? cell.stripes : cell.stripes.filter(stripe => stripe.hue === hue);
    }

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

    PlasmaComponents.Label {
        visible: block.stacked
        text: block.stacked ? heatmap.layers[0].name : ""
        font: Kirigami.Theme.smallFont
        opacity: Theme.secondaryOpacity
    }
    Grid {
        id: grid
        objectName: "heatmapGrid"
        activeFocusOnTab: true
        focus: true
        Accessible.role: Accessible.Table
        Accessible.name: "Heatmap, " + (block.focusedIndex >= 0 ? block.lineAt(block.focusedIndex) : "")
        onActiveFocusChanged: if (activeFocus && block.focusedIndex < 0) block.focusedIndex = heatmap.cells.length - 1
        Keys.onPressed: event => {
            const last = heatmap.cells.length - 1;
            let next = block.focusedIndex;
            if (event.key === Qt.Key_Left) next -= 7;
            else if (event.key === Qt.Key_Right) next += 7;
            else if (event.key === Qt.Key_Up) next -= 1;
            else if (event.key === Qt.Key_Down) next += 1;
            else if (event.key === Qt.Key_Home) next = 0;
            else if (event.key === Qt.Key_End) next = last;
            else return;
            block.focusedIndex = Math.max(0, Math.min(last, next));
            event.accepted = true;
        }
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
                required property int index
                readonly property var shown: block.stripesOf(modelData, block.firstHue)
                width: grid.side
                height: grid.side
                radius: 2
                clip: true
                color: shown.length === 0 ? Qt.alpha(Kirigami.Theme.textColor, Theme.emptyCellOpacity)
                                                      : "transparent"
                Accessible.name: modelData.line
                border.width: grid.activeFocus && index === block.focusedIndex ? 2 : 0
                border.color: Kirigami.Theme.highlightColor

                Row {
                    anchors.fill: parent
                    Repeater {
                        model: cell.shown
                        Rectangle {
                            required property var modelData
                            width: grid.side / cell.shown.length
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

    Repeater {
        model: block.stacked ? heatmap.layers.slice(1) : []
        ColumnLayout {
            id: layerGrid
            required property var modelData
            Layout.fillWidth: true
            spacing: 6

            PlasmaComponents.Label {
                text: layerGrid.modelData.name
                font: Kirigami.Theme.smallFont
                opacity: Theme.secondaryOpacity
            }
            Grid {
                Layout.fillWidth: true
                Layout.preferredHeight: 7 * grid.side + 6 * Theme.cellGap
                rows: 7
                flow: Grid.TopToBottom
                spacing: Theme.cellGap
                Accessible.name: "Heatmap, " + layerGrid.modelData.name

                Repeater {
                    model: heatmap.cells
                    Rectangle {
                        id: stackedCell
                        required property var modelData
                        readonly property var shown: block.stripesOf(modelData, layerGrid.modelData.hue)
                        width: grid.side
                        height: grid.side
                        radius: 2
                        color: shown.length === 0 ? Qt.alpha(Kirigami.Theme.textColor, Theme.emptyCellOpacity)
                                                  : Theme.hue(shown[0].hue)
                        opacity: shown.length === 0 ? 1 : Theme.levelOpacity(shown[0].level) * (shown[0].dimmed ? 0.5 : 1)
                        Accessible.name: modelData.line
                    }
                }
            }
        }
    }

    PlasmaComponents.Label {
        objectName: "heatmapLine"
        text: block.focusedIndex >= 0 && grid.activeFocus ? block.lineAt(block.focusedIndex)
            : (hoveredLine.length > 0 ? hoveredLine : block.lineAt(block.lastIndex))
        font: Kirigami.Theme.smallFont
        opacity: Theme.secondaryOpacity
        elide: Text.ElideRight
        Layout.fillWidth: true
    }
}
