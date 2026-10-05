import QtQuick
import org.kde.kirigami as Kirigami

// FR-51 `heatmap` and `sharedHeatmap`: the last three weeks, rows are weeks from Sunday. `onlyHue` keeps the label
// source's stripes (heatmap); empty keeps every layer's (sharedHeatmap).
Grid {
    id: mini

    property var cells: []
    property string onlyHue: ""
    readonly property real side: Math.floor((Kirigami.Units.iconSizes.small - 2 * spacing) / 3)
    // The last three week rows of heatmapDays(): its cells start on a Sunday, one column per week.
    readonly property var recent: {
        const n = cells.length;
        if (n === 0) {
            return [];
        }
        const start = Math.max(0, n - ((n - 1) % 7) - 1 - 14);
        return cells.slice(start);
    }

    rows: 3
    columns: 7
    spacing: 1

    Repeater {
        model: 21
        Rectangle {
            required property int index
            readonly property var cell: index < mini.recent.length ? mini.recent[index] : null
            readonly property var stripes: cell ? cell.stripes.filter(s => mini.onlyHue.length === 0 || s.hue === mini.onlyHue) : []
            objectName: "miniCell"
            width: mini.side
            height: mini.side
            radius: 1
            clip: true
            color: stripes.length === 0 ? Qt.alpha(Kirigami.Theme.textColor, cell ? Theme.emptyCellOpacity : 0) : "transparent"
            Row {
                anchors.fill: parent
                Repeater {
                    model: parent.parent.stripes
                    Rectangle {
                        required property var modelData
                        width: mini.side / Math.max(1, parent.parent.stripes.length)
                        height: mini.side
                        color: Theme.hue(modelData.hue)
                        opacity: Theme.levelOpacity(modelData.level)
                    }
                }
            }
        }
    }
}
