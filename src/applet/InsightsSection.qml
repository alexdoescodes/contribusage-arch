import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kde.plasma.components as PlasmaComponents

// FR-38: collapsed by default; one period at a time, its shares, then the top three of each ranking.
ColumnLayout {
    id: section

    property var insights
    // Owned by the popup, so the open state and the period survive the rows being rebuilt every minute.
    property QtObject viewState: null
    readonly property bool expanded: viewState ? viewState.expanded : localExpanded
    readonly property int period: Math.min(viewState ? viewState.period : localPeriod, insights.periods.length - 1)
    readonly property var shown: insights.periods[period]
    property bool localExpanded: false
    property int localPeriod: 0

    function toggle() {
        if (viewState) {
            viewState.expanded = !expanded;
        } else {
            localExpanded = !expanded;
        }
    }

    function selectPeriod(index) {
        if (viewState) {
            viewState.period = index;
        } else {
            localPeriod = index;
        }
    }

    spacing: 4

    PlasmaComponents.ToolButton {
        text: "Insights"
        icon.name: parent.expanded ? "arrow-down" : "arrow-right"
        onClicked: section.toggle()
        QQC2.ToolTip.text: insights.note
        QQC2.ToolTip.visible: hovered
        QQC2.ToolTip.delay: 200
    }
    ColumnLayout {
        visible: parent.expanded
        spacing: 3
        Layout.fillWidth: true

        PlasmaComponents.TabBar {
            currentIndex: period
            Layout.fillWidth: true
            onCurrentIndexChanged: section.selectPeriod(currentIndex)

            Repeater {
                model: insights.periods
                PlasmaComponents.TabButton {
                    required property var modelData
                    text: modelData.label
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
