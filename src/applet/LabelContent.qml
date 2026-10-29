import QtQuick
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kde.plasma.components as PlasmaComponents

// SPEC §11.1, FR-51: the panel label in one of seven styles, from plain values, so it can be tested without Plasma.
RowLayout {
    id: content

    property string style: "prompt"
    property string tint: "provider"
    property color customColor: Theme.claudeHue
    property var label: ({})
    property var heatmap: ({})
    property bool vertical: false

    readonly property var styles: ["prompt", "rings", "ring", "line", "heatmap", "sharedHeatmap", "text"]
    readonly property string shownStyle: styles.indexOf(style) >= 0 ? style : "prompt"
    readonly property string hueName: label.source === "github" ? "github" : "claude"
    readonly property color meterColor: Theme.labelColor(tint, label.level, hueName, customColor,
                                                         Kirigami.Theme.textColor, Kirigami.Theme.highlightColor)
    readonly property real meterOpacity: label.stale ? 0.5 : 1
    // The heatmap styles show activity, so the value turns red instead of the meter (FR-51).
    readonly property bool heatmapStyle: shownStyle === "heatmap" || shownStyle === "sharedHeatmap"
    readonly property bool showsValue: !vertical && shownStyle !== "ring" && shownStyle !== "text"

    spacing: 4
    Accessible.name: (label.title ?? "") + " " + (label.text ?? "")

    PromptGlyph {
        // A vertical panel has no room for the text style's name and value: it shows the mark.
        visible: content.shownStyle === "prompt" || (content.shownStyle === "text" && content.vertical)
        fraction: content.label.fraction ?? 0
        fillColor: content.meterColor
        opacity: content.meterOpacity
        Layout.preferredHeight: Kirigami.Units.iconSizes.small
        Layout.preferredWidth: implicitWidth
    }
    RingGlyph {
        visible: content.shownStyle === "rings" || content.shownStyle === "ring"
        fraction: content.label.fraction ?? 0
        inner: content.shownStyle === "rings" ? (content.label.companion ?? -1) : -1
        centerText: content.shownStyle === "ring" ? (content.label.text ?? "").replace("%", "").trim() : ""
        fillColor: content.meterColor
        opacity: content.meterOpacity
    }
    LineGlyph {
        visible: content.shownStyle === "line"
        fraction: content.label.fraction ?? 0
        fillColor: content.meterColor
        opacity: content.meterOpacity
    }
    MiniHeatmap {
        visible: content.heatmapStyle
        cells: content.heatmap.cells ?? []
        onlyHue: content.shownStyle === "heatmap" ? content.hueName : ""
    }
    PlasmaComponents.Label {
        objectName: "labelText"
        visible: content.showsValue || (content.shownStyle === "text" && !content.vertical)
        text: content.shownStyle === "text" ? (content.label.title ?? "") + " " + (content.label.text ?? "").trim()
                                            : (content.label.text ?? "")
        color: content.heatmapStyle && content.label.level === "critical" ? Theme.critical : Kirigami.Theme.textColor
        font.features: ({ "tnum": 1 })
    }
}
