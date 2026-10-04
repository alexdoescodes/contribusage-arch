import QtQuick
import QtQuick.Layouts
import org.kde.plasma.components as PlasmaComponents

// One provider: header, limits, activity, insights (FR-4, SPEC §11.2).
ColumnLayout {
    property var provider
    property QtObject insightsState: null

    spacing: Theme.spacing

    SectionHeader {
        title: provider.name
        symbol: provider.symbol
        updatedText: provider.updatedText
        Layout.fillWidth: true
    }
    PlasmaComponents.Label {
        visible: (provider.statusText ?? "").length > 0
        text: provider.statusText ?? ""
        wrapMode: Text.Wrap
        opacity: Theme.secondaryOpacity
        Layout.fillWidth: true
    }
    Repeater {
        model: provider.windows
        LimitBar {
            required property var modelData
            limit: modelData
            opacity: provider.stale ? 0.5 : 1
            Layout.fillWidth: true
        }
    }
    Loader {
        active: provider.activity !== undefined
        visible: active
        Layout.fillWidth: true
        sourceComponent: ActivitySection { activity: provider.activity }
    }
    Loader {
        active: provider.insights !== undefined
        visible: active
        Layout.fillWidth: true
        sourceComponent: InsightsSection {
            insights: provider.insights
            viewState: insightsState
        }
    }
}
