import QtQuick
import QtQuick.Layouts

// One provider: header, limits, activity, insights (FR-4, SPEC §11.2).
ColumnLayout {
    property var provider

    spacing: Theme.spacing

    SectionHeader {
        title: provider.name
        symbol: provider.symbol
        updatedText: provider.updatedText
        Layout.fillWidth: true
    }
    Repeater {
        model: provider.windows
        LimitBar {
            required property var modelData
            limit: modelData
            Layout.fillWidth: true
        }
    }
    ActivitySection {
        activity: provider.activity
        Layout.fillWidth: true
    }
    InsightsSection {
        insights: provider.insights
        Layout.fillWidth: true
    }
}
