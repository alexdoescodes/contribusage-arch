import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kde.plasma.components as PlasmaComponents

// FR-38: collapsed by default; one period at a time, its shares, then the top three of each ranking.
ColumnLayout {
    property var insights
    property bool expanded: false
    property int period: 0
    readonly property var shown: insights.periods[period]

    spacing: 4

    PlasmaComponents.ToolButton {
        text: "Insights"
        icon.name: parent.expanded ? "arrow-down" : "arrow-right"
        onClicked: parent.expanded = !parent.expanded
        QQC2.ToolTip.text: insights.note
        QQC2.ToolTip.visible: hovered
        QQC2.ToolTip.delay: 200
    }
    ColumnLayout {
        visible: parent.expanded
        spacing: 3
        Layout.fillWidth: true

        RowLayout {
            Repeater {
                model: insights.periods
                PlasmaComponents.TabButton {
                    required property var modelData
                    required property int index
                    text: modelData.label
                    checked: index === period
                    onClicked: period = index
                }
            }
        }
        PlasmaComponents.Label { text: shown.summary; font: Kirigami.Theme.smallFont; opacity: Theme.secondaryOpacity }
        Repeater {
            model: shown.rows
            delegate: shareRow
        }
        Repeater {
            model: shown.rankings
            ColumnLayout {
                required property var modelData
                spacing: 3
                Layout.fillWidth: true
                PlasmaComponents.Label {
                    text: modelData.title
                    font: Kirigami.Theme.smallFont
                    opacity: Theme.secondaryOpacity
                    Layout.topMargin: 5
                }
                Repeater {
                    model: modelData.rows
                    delegate: shareRow
                }
            }
        }
    }

    Component {
        id: shareRow
        RowLayout {
            required property var modelData
            Layout.fillWidth: true
            PlasmaComponents.Label { text: modelData.label; elide: Text.ElideMiddle; Layout.fillWidth: true }
            PlasmaComponents.Label { text: modelData.percent; opacity: Theme.secondaryOpacity }
        }
    }
}
