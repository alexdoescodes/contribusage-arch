import QtQuick
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kde.plasma.components as PlasmaComponents

// US-5: today's numbers, the token categories, then total tokens of the last 7 days, today in full (SPEC §11.2).
ColumnLayout {
    property var activity

    spacing: 4

    PlasmaComponents.Label { text: "On this machine"; opacity: Theme.secondaryOpacity }
    PlasmaComponents.Label { text: activity.todayText }
    PlasmaComponents.Label {
        text: activity.categoriesText
        font: Kirigami.Theme.smallFont
        opacity: Theme.secondaryOpacity
        wrapMode: Text.Wrap
        Layout.fillWidth: true
    }
    RowLayout {
        Layout.fillWidth: true

        Row {
            id: bars
            spacing: Theme.spacing
            Layout.fillWidth: true
            Layout.preferredHeight: Theme.weekChartHeight
            Accessible.name: "Tokens per day, last 7 days"

            Repeater {
                model: activity.week
                Rectangle {
                    required property var modelData
                    width: (bars.width - bars.spacing * 6) / 7
                    height: Math.max(2, Theme.weekChartHeight * modelData.fraction)
                    anchors.bottom: parent.bottom
                    radius: 2
                    color: Theme.accent
                    opacity: modelData.today ? 1 : Theme.mutedBarOpacity
                }
            }
        }
        PlasmaComponents.Label {
            text: "last 7 days"
            font: Kirigami.Theme.smallFont
            opacity: Theme.secondaryOpacity
            Layout.alignment: Qt.AlignBottom
        }
    }
}
