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
    readonly property var periods: insights && insights.periods ? insights.periods : []
    readonly property int period: Math.max(0, Math.min(viewState ? viewState.period : localPeriod, periods.length - 1))
    // Empty while a provider update hands over no insights for a moment.
    readonly property var shown: periods[period] ?? ({})
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
        // SwiftUI's DisclosureGroup under macOS: a small chevron before the title.
        icon.name: Theme.iconSource(parent.expanded ? "arrow-down" : "arrow-right").length > 0 ? "" : (parent.expanded ? "arrow-down" : "arrow-right")
        icon.source: Theme.iconSource(parent.expanded ? "arrow-down" : "arrow-right")
        icon.color: Kirigami.Theme.textColor
        icon.width: Theme.macOS ? 10 : Kirigami.Units.iconSizes.small
        icon.height: Theme.macOS ? 10 : Kirigami.Units.iconSizes.small
        onClicked: section.toggle()
        QQC2.ToolTip.text: insights?.note ?? ""
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
                model: section.periods
                PlasmaComponents.TabButton {
                    required property var modelData
                    text: modelData.label
                }
            }
        }
        PlasmaComponents.Label { topPadding: Theme.labelInset; bottomPadding: Theme.labelInset; text: shown.summary ?? ""; font: Kirigami.Theme.smallFont; opacity: Theme.secondaryOpacity }
        Repeater {
            model: shown.rows ?? []
            delegate: shareRow
        }
        Repeater {
            model: shown.rankings ?? []
            ColumnLayout {
                required property var modelData
                spacing: 3
                Layout.fillWidth: true
                PlasmaComponents.Label {
                    topPadding: Theme.labelInset
                    bottomPadding: Theme.labelInset
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
            PlasmaComponents.Label { topPadding: Theme.labelInset; bottomPadding: Theme.labelInset; text: modelData.label; elide: Text.ElideMiddle; Layout.fillWidth: true }
            PlasmaComponents.Label { topPadding: Theme.labelInset; bottomPadding: Theme.labelInset; text: modelData.percent; opacity: Theme.secondaryOpacity }
        }
    }
}
